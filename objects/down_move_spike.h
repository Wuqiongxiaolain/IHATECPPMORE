#pragma once
#include "base_object.h"
#include "act_seq.h"

class DownMoveSpike : public BaseObject {
public:
	DownMoveSpike() noexcept : BaseObject() {}
	~DownMoveSpike() noexcept {}
	void Start() override;
	void Update() override;
	void OnCollisionEnter(const ObjManager::ObjToken& other_token, const CF_Manifold& manifold) noexcept override;

private:
	ActSeq m_act_seq;
};