#include "test_object.h"

static constexpr float force = 1.0f;

void TestObject::Start()
{
	// ��������ͼ·������������ʹ��Ĭ��ֵ
	SpriteSetSource("/sprites/bullet.png", 1);
}

void TestObject::Update()
{
	// 使用 BaseObject 暴露的物理接口来修改速度/受力：
	// 1) 根据按键添加力（SetForce）
	// 2) 将力积分到速度（ApplyForce）
	// 3) 可选阻尼（SetVelocity）
	// 4) 将速度积分到位置（ApplyVelocity）

	//Scale(3.0f);  // �Ŵ�2��������Ը�����Ҫ������ֵ

	int dir = 0;
	if (cf_key_down(CF_KEY_LEFT)) {
		dir--;
	}
	if (cf_key_down(CF_KEY_RIGHT)) {
		dir++;
	}

	// 添加力
	SetForce(dir * v2math::angled(CF_V2(force), GetRotation()));

	// 简单水平方向阻尼：无按键时减速，避免无限滑动
	// 这里使用已有的速度接口来读取/写入速度
	CF_V2 vel = GetVelocity();
	constexpr float damping = 0.90f;
	if (!cf_key_down(CF_KEY_LEFT) && !cf_key_down(CF_KEY_RIGHT)) {
		vel *= damping;
		SetVelocity(vel);
	}*/
}