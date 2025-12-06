#include "obj_manager.h"
#include "base_object.h" // 提供 BaseObject 声明
#include <typeinfo>
#include <iostream>
#include <cstdint>
#include <limits>
#include <stdexcept>

ObjManager::ObjManager() noexcept = default;

ObjManager::~ObjManager() noexcept
{
    // 析构时依赖 unique_ptr 自动释放管理的对象资源
}

ObjManager & ObjManager::Instance() noexcept
{
    static ObjManager inst;
    return inst;
}

BaseObject* ObjManager::Get(const ObjToken& token) noexcept
{
    if (token.index >= objects_.size()) return nullptr;
    const Entry& e = objects_[token.index];
    if (!e.alive) return nullptr;
    if (e.generation != token.generation) return nullptr;
    return e.ptr.get();
}

bool ObjManager::IsValid(const ObjToken& token) const noexcept
{
    if (token.index >= objects_.size()) return false;
    const Entry& e = objects_[token.index];
    return e.alive && (e.generation == token.generation) && e.ptr;
}

// 为延迟创建预留 slot，如果有空闲索引则复用，否则在末尾追加新条目。
uint32_t ObjManager::ReserveSlotForCreate() noexcept
{
    if (!free_indices_.empty()) {
        uint32_t idx = free_indices_.back();
        free_indices_.pop_back();
        // 确保 slot 中的 unique_ptr 被重置，alive 标志置 false
        Entry& e = objects_[idx];
        e.ptr.reset();
        e.alive = false;
        e.skip_update_this_frame = false;
        // generation 将在 CreateEntry/Delayed 时增加
        return idx;
    } else {
        objects_.emplace_back();
        return static_cast<uint32_t>(objects_.size() - 1);
    }
}

// 将 unique_ptr<BaseObject> 纳入管理并立即启动（Start），但不直接扩展 objects_；
// 对象被放入 pending_creates_，在 UpdateAll 的提交阶段合并到 objects_（安全点）。
ObjManager::PendingToken ObjManager::CreateEntry(std::unique_ptr<BaseObject> obj)
{
    if (!obj) {
        std::cerr << "[InstanceController] CreateEntry: factory returned nullptr\n";
        return PendingToken::Invalid();
    }

    BaseObject* raw = obj.get();

    // 立即调用 Start()，但要注意异常安全：若 Start() 失败需要回滚
    try {
        raw->Start();
    }
    catch (...) {
        std::cerr << "[InstanceController] CreateEntry: Start() threw for object at " << static_cast<const void*>(raw) << " (pending)\n";
        return PendingToken::Invalid();
    }

    // 分配 pending id 并将对象放入 pending 创建区；此时不向 objects_ 添加条目以避免在更新循环中触发 vector 重分配导致迭代器失效。
    uint32_t pid = next_pending_id_++;
    pending_creates_.emplace(pid, PendingCreate{ std::move(obj) });
    pending_ptr_to_id_.emplace(raw, pid);
    ++alive_count_;

    std::cerr << "[InstanceController] CreateEntry: created pending object at " << static_cast<const void*>(raw)
        << " (pending id=" << pid << ", commit next-frame)\n";

    PendingToken token;
    token.id = pid;
    return token;
}

// 内部按索引立即销毁条目：调用 OnDestroy、反注册物理系统、释放资源并使 token 失效
void ObjManager::DestroyEntry(uint32_t index) noexcept
{
    if (index >= objects_.size()) return;
    Entry& e = objects_[index];
    if (!e.alive || !e.ptr) return;

    BaseObject* raw = e.ptr.get();
    std::cerr << "[InstanceController] DestroyEntry: destroying object at "
        << static_cast<const void*>(raw) << " (type: " << typeid(*raw).name() << ", index=" << index
        << ", gen=" << e.generation << ")\n";

    // 先从物理系统反注册（如果此前未注册，Unregister 应为 no-op）
    ObjManager::ObjToken tok{ index, e.generation };
    PhysicsSystem::Instance().Unregister(tok);

    // 调用对象的销毁钩子以便对象处理自身资源
    e.ptr->OnDestroy();

    // 从索引映射中移除
    object_index_map_.erase(raw);

    // 将对象的 token 设为 Invalid，避免悬挂句柄
    e.ptr->SetObjToken(ObjToken::Invalid());

    // 释放 unique_ptr 并标记 slot 可复用
    e.ptr.reset();
    e.alive = false;
    e.skip_update_this_frame = false;

    // 增加 generation 使旧 token 失效
    ++e.generation;

    free_indices_.push_back(index);

    if (alive_count_ > 0) --alive_count_;
}

void ObjManager::Destroy(BaseObject* ptr) noexcept
{
    if (!ptr) return;

    // 先尝试通过映射找到索引并构造 token（仅包含已合并到 objects_ 的对象）
    auto it = object_index_map_.find(ptr);
    if (it != object_index_map_.end()) {
        uint32_t index = it->second;
        if (index < objects_.size() && objects_[index].ptr.get() == ptr) {
            ObjToken tok{ index, objects_[index].generation };
            Destroy(tok); // 使用延迟销毁路径入队
            return;
        }
    }

    // 如果对象尚处于 pending 创建区，直接移除并销毁
    auto pit = pending_ptr_to_id_.find(ptr);
    if (pit != pending_ptr_to_id_.end()) {
        uint32_t pid = pit->second;
        auto pcit = pending_creates_.find(pid);
        if (pcit != pending_creates_.end()) {
            // 调用 OnDestroy 让对象清理自身资源
            pcit->second.ptr->OnDestroy();
            // 若意外存在 token，置为 Invalid（通常 pending 对象尚未被赋 token）
            pcit->second.ptr->SetObjToken(ObjToken::Invalid());
            // 移除 pending 记录（unique_ptr 被释放）
            pending_creates_.erase(pcit);
            pending_ptr_to_id_.erase(pit);
            if (alive_count_ > 0) --alive_count_;
            std::cerr << "[InstanceController] Destroy: destroyed pending object at " << static_cast<const void*>(ptr) << " (pending id=" << pid << ")\n";
            return;
        }
    }

    // 映射未命中则做线性查找以兼容异常情况（仅检查已合并对象）
    for (uint32_t i = 0; i < objects_.size(); ++i) {
        Entry& e = objects_[i];
        if (e.ptr && e.ptr.get() == ptr) {
            ObjToken tok{ i, e.generation };
            Destroy(tok); // 使用延迟销毁路径入队
            return;
        }
    }

    std::cerr << "[InstanceController] Destroy: target not found at "
              << static_cast<const void*>(ptr) << "\n";
}

void ObjManager::Destroy(const ObjToken& token) noexcept
{
    if (token.index >= objects_.size()) {
        std::cerr << "[InstanceController] Destroy(token): invalid index " << token.index << "\n";
        return;
    }
    const Entry& e = objects_[token.index];
    if (!e.alive || e.generation != token.generation) {
        std::cerr << "[InstanceController] Destroy(token): token invalid or object not alive (index="
            << token.index << ", gen=" << token.generation << ")\n";
        return;
    }

    // 使用 compact key 去重，避免重复入队
    uint64_t key = (static_cast<uint64_t>(token.index) << 32) | token.generation;
    if (pending_destroy_set_.insert(key).second) {
        pending_destroys_.push_back(token);
        std::cerr << "[InstanceController] Destroy: enqueued destroy for index=" << token.index
            << " gen=" << token.generation << "\n";
    }
    else {
        std::cerr << "[InstanceController] Destroy: already enqueued for index=" << token.index
            << " gen=" << token.generation << "\n";
    }
}

void ObjManager::DestroyAll() noexcept
{
    std::cerr << "[InstanceController] DestroyAll: destroying all objects (" << alive_count_ << ")\n";

    // 清理所有挂起的创建/销毁队列
    pending_destroys_.clear();
    pending_destroy_set_.clear();
    pending_creates_.clear();
    pending_ptr_to_id_.clear();
    pending_to_real_map_.clear();

    // 先从物理系统统一反注册所有仍然存活的对象
    for (uint32_t i = 0; i < objects_.size(); ++i) {
        Entry& e = objects_[i];
        if (e.alive && e.ptr) {
            PhysicsSystem::Instance().Unregister(ObjToken{ i, e.generation });
        }
    }

    // 调用 OnDestroy 并彻底释放所有对象资源，使 token 失效
    for (uint32_t i = 0; i < objects_.size(); ++i) {
        Entry& e = objects_[i];
        if (e.alive && e.ptr) {
            e.ptr->OnDestroy();
            // 置 token 为 Invalid
            e.ptr->SetObjToken(ObjToken::Invalid());
            object_index_map_.erase(e.ptr.get());
            e.ptr.reset();
            e.alive = false;
            e.skip_update_this_frame = false;
            ++e.generation; // 使旧 token 失效
            free_indices_.push_back(i);
        }
    }

    objects_.clear();
    free_indices_.clear();
    object_index_map_.clear();
    alive_count_ = 0;
}

void ObjManager::UpdateAll() noexcept
{
    // 1) 应用物理更新：为每个活跃对象调用 FramelyApply()
    // 使用索引遍历以避免持有范围 for 中的引用而在并发修改/重分配时失效
    for (size_t i = 0; i < objects_.size(); ++i) {
        Entry& e = objects_[i];
        if (e.alive && e.ptr && !e.skip_update_this_frame) { e.ptr->FramelyApply(); }
    }

    // 2) 全局碰撞检测与回调
    PhysicsSystem::Instance().Step();

    // 3) 每帧为活跃对象调用 Update()
    for (size_t i = 0; i < objects_.size(); ++i) {
        Entry& e = objects_[i];
        if (e.alive && e.ptr && !e.skip_update_this_frame) { e.ptr->Update(); }
    }

    // 4) 执行延迟销毁队列（在更新循环安全点处理）
    if (!pending_destroys_.empty()) {
        for (const ObjToken& token : pending_destroys_) {
            if (token.index >= objects_.size()) {
                std::cerr << "[InstanceController] UpdateAll: pending destroy invalid index " << token.index << "\n";
                continue;
            }
            Entry& e = objects_[token.index];
            if (!e.alive || e.generation != token.generation) {
                std::cerr << "[InstanceController] UpdateAll: pending destroy target not found or token mismatch (index="
                    << token.index << ", gen=" << token.generation << ")\n";
                continue;
            }

            std::cerr << "[InstanceController] UpdateAll: executing destroy for object at index=" << token.index
                << " gen=" << token.generation << " (type: " << typeid(*e.ptr).name() << ")\n";

            // 释放该 slot
            DestroyEntry(token.index);
        }

        pending_destroys_.clear();
        pending_destroy_set_.clear();
    }

    // 5) 提交本帧 pending 的创建：在安全点把 pending_creates_ 合并到 objects_ 并注册物理系统，
    //    使其在下一帧参与 FramelyApply / Update / 物理处理。
    if (!pending_creates_.empty()) {
        // 预留容量以避免在合并过程中发生多次重分配
        objects_.reserve(objects_.size() + pending_creates_.size());

        // 收集 pending id 列表，避免在循环中修改 unordered_map 导致迭代问题
        std::vector<uint32_t> pids;
        pids.reserve(pending_creates_.size());
        for (const auto &kv : pending_creates_) pids.push_back(kv.first);

        for (uint32_t pid : pids) {
            auto it = pending_creates_.find(pid);
            if (it == pending_creates_.end()) continue;
            PendingCreate &pc = it->second;
            if (!pc.ptr) {
                pending_creates_.erase(it);
                continue;
            }

            BaseObject* raw = pc.ptr.get();
            uint32_t index = 0;
            // 复用空闲 slot 或在末尾追加
            if (!free_indices_.empty()) {
                index = free_indices_.back();
                free_indices_.pop_back();
                Entry& e = objects_[index];
                e.ptr = std::move(pc.ptr);
                e.alive = true;
                ++e.generation;
                // 合并到 objects_ 后应在下一帧参与更新，因此这里不设置 skip
                e.skip_update_this_frame = false;
            } else {
                objects_.emplace_back();
                index = static_cast<uint32_t>(objects_.size() - 1);
                Entry& e = objects_[index];
                e.ptr = std::move(pc.ptr);
                e.alive = true;
                ++e.generation;
                e.skip_update_this_frame = false;
            }

            // 注册索引映射并注册到物理系统
            object_index_map_[raw] = index;
            ObjManager::ObjToken tok{ index, objects_[index].generation };
            PhysicsSystem::Instance().Register(tok, raw);

            // 将真实 token 写入对象（ObjManager 为 friend，允许调用 private SetObjToken）
            if (objects_[index].ptr) {
                objects_[index].ptr->SetObjToken(tok);
            }

            // 记录 pending -> real 的映射，便于 ResolvePending
            pending_to_real_map_.emplace(pid, tok);

            // 从 pending_ptr_to_id_ 中移除
            pending_ptr_to_id_.erase(raw);

            std::cerr << "[InstanceController] UpdateAll: committed pending object at " << static_cast<const void*>(raw)
                << " (type: " << typeid(*objects_[index].ptr).name() << ", pending id=" << pid << ", index=" << index << ", gen=" << objects_[index].generation << ")\n";

            // 从 pending_creates_ 中移除该条目
            pending_creates_.erase(it);
        }
    }
}

ObjManager::ObjToken ObjManager::ResolvePending(const PendingToken& p) const noexcept
{
    if (!p.IsValid()) return ObjToken::Invalid();
    auto it = pending_to_real_map_.find(p.id);
    if (it != pending_to_real_map_.end()) {
        return it->second;
    }
    // 尚未合并或不存在，返回 Invalid
    return ObjToken::Invalid();
}

void ObjManager::DestroyPending(const PendingToken& p) noexcept
{
    if (!p.IsValid()) return;
    auto it = pending_creates_.find(p.id);
    if (it == pending_creates_.end()) return;
    BaseObject* raw = it->second.ptr.get();
    // 调用 OnDestroy 让对象清理自身资源
    it->second.ptr->OnDestroy();
    // 若意外存在 token，置为 Invalid（通常 pending 对象尚未被赋 token）
    it->second.ptr->SetObjToken(ObjToken::Invalid());
    // 移除 pending 记录
    pending_creates_.erase(it);
    pending_ptr_to_id_.erase(raw);
    if (alive_count_ > 0) --alive_count_;
    std::cerr << "[InstanceController] DestroyPending: destroyed pending id=" << p.id << " at " << static_cast<const void*>(raw) << "\n";
}

// operator[] 实现：若 token 无效或对象不可用，则抛出 std::out_of_range（并写入 std::cerr）
BaseObject& ObjManager::operator[](const ObjToken& token)
{
    if (token.index >= objects_.size()) {
        std::cerr << "[InstanceController] operator[]: invalid index " << token.index << "\n";
        throw std::out_of_range("ObjManager::operator[]: invalid index");
    }
    Entry& e = objects_[token.index];
    if (!e.alive || e.generation != token.generation || !e.ptr) {
        std::cerr << "[InstanceController] operator[]: token invalid or object not alive (index=" << token.index << ", gen=" << token.generation << ")\n";
        throw std::out_of_range("ObjManager::operator[]: token invalid or object not alive");
    }
    return *e.ptr;
}

const BaseObject& ObjManager::operator[](const ObjToken& token) const
{
    if (token.index >= objects_.size()) {
        std::cerr << "[InstanceController] operator[] const: invalid index " << token.index << "\n";
        throw std::out_of_range("ObjManager::operator[] const: invalid index");
    }
    const Entry& e = objects_[token.index];
    if (!e.alive || e.generation != token.generation || !e.ptr) {
        std::cerr << "[InstanceController] operator[] const: token invalid or object not alive (index=" << token.index << ", gen=" << token.generation << ")\n";
        throw std::out_of_range("ObjManager::operator[] const: token invalid or object not alive");
    }
    return *e.ptr;
}