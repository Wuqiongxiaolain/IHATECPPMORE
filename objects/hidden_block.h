#pragma once
#include "base_object.h"

class HiddenBlock : public BaseObject {
public:
    HiddenBlock(CF_V2 pos) noexcept : BaseObject(), position(pos) {}
    ~HiddenBlock() noexcept {}

    void Start() override;

    void OnCollisionEnter(const ObjManager::ObjToken& other_token, const CF_Manifold& manifold) noexcept override;
private:
    CF_V2 position{ 0.0f, 0.0f };
    bool once;
};
