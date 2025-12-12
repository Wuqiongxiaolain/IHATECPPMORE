#include "checkpoint.h"

void Checkpoint::Start()
{
    // 把对象放到传入的位置
    SetPosition(position);

    // 标记为 checkpoint，便于 ObjManager 的 FindTokensByTag 查找
	//!!!之后要改到update里面，挡子弹打到时再打标签
    //! 
    //! 
    AddTag("checkpoint");

    // 不参与物理碰撞（仅作标记 / 可视化）
    SetColliderType(ColliderType::VOID);

	// 设置精灵资源（静态贴图）
     SpriteSetSource("/sprites/Save_red.png", 1, false);
     SpriteSetUpdateFreq(1);
}

void Checkpoint::Update()
{
    
}