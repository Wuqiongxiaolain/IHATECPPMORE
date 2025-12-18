#pragma once
#include "base_object.h"
#include "act_seq.h"

class StraightCherry : public BaseObject {
public:
    StraightCherry(CF_V2 initial_pos=CF_V2(0.0f,0.0f), float move_distance=200.0f, bool dir_up = true) noexcept 
        : BaseObject(), initial_position(initial_pos), move_distance(move_distance), direction_up(dir_up) {}
    ~StraightCherry() noexcept override {}

    bool GetDirction() { return direction_up; }

    void Start() override;
    void Update() override;
    void OnCollisionEnter(const ObjManager::ObjToken& other_token, const CF_Manifold& manifold) noexcept override;

private:
    ActSeq m_act_seq;
    CF_V2 initial_position;
    float move_distance = 200.0f; // 直向移动的总距离
	bool direction_up; // 是否向上
};