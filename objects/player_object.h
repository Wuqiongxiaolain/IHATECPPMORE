#pragma once
#include "base_object.h"
#include <iostream>
#include <cmath>
#include <unordered_map>
#include <cute.h>

class PlayerObject : public BaseObject {
public:
    PlayerObject() noexcept : BaseObject() {}
    ~PlayerObject() noexcept {}

    void Start() override;
    void Update() override;
	void EndFrame() override;

    void Exclusion(const CF_Manifold& m);

	void OnCollisionEnter(const ObjManager::ObjToken& other_token, const CF_Manifold& manifold) noexcept override;
	void OnCollisionStay(const ObjManager::ObjToken& other_token, const CF_Manifold& manifold) noexcept override;
	void OnCollisionExit(const ObjManager::ObjToken& other_token, const CF_Manifold& manifold) noexcept override;
private:
	bool grounded = false;
	float hold_time_left = 0.0f;
	int coyote_time_left = 0;
	int jump_count = 2;

	// “Ù–ß◊ ‘¥
	CF_Audio jump_sound_;
	CF_Audio shoot_sound_;
	CF_Audio double_jump_sound_;
	CF_Audio land_sound_;
};