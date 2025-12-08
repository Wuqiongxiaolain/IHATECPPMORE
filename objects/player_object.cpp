#include "player_object.h"
#include "test_object.h"

// 每帧移动的速度
static constexpr float speed = 3.0f;
const float PI = 3.14159265358979f;

// �ļ���ӳ�䣺Ϊÿ�� PlayerObject ʵ�������Ƿ��ŵ�
static std::unordered_map<const PlayerObject*, bool> s_grounded_map;

// �ļ���ӳ�䣺Ϊÿ��ʵ�����水ס��Ծ���õ�ʣ��֡����variable jump��
static std::unordered_map<const PlayerObject*, float> s_jump_hold_time_left;

// �ļ���ӳ�䣺Ϊÿ��ʵ�������뿪�����������Ծ�� coyote ʱ�䣨֡��
static std::unordered_map<const PlayerObject*, float> s_coyote_time_left;

// �ɵ�����
static constexpr int max_jump_hold_frames = 12;       // ��ס��Ծ���������֡������סԽ������Խ�ߣ�
static constexpr float low_gravity_multiplier = 0.4f; // ��ס��Ծʱ���������ʣ���С�����������ٶȣ�
static constexpr float fall_gravity_multiplier = 1.6f; // �ɿ�������ʱ�ļ�ǿ�������ʣ��������䣩
static constexpr int coyote_time_frames = 6;         // ��غ��Կ�����֡����coyote time��

ObjToken test[3] = { ObjToken::Invalid() };
int i = 0;
void PlayerObject::Start()
{
	// 统一设置贴图路径、竖排帧数、动画更新频率和绘制深度，并注册到绘制序列
	// 如需要默认值，请使用高粒度的 SetSprite*() 和 Set*() 方法逐一设置非默认值参数
	// 资源路径无默认值，必须手动设置
    SpriteSetStats("/sprites/idle.png", 3, 7, 0);

    // 可选：初始化位置（根据需要调整），例如屏幕中心附近
    SetPosition(cf_v2(0.0f, 0.0f));

    Scale(0.5f);
	SetPivot(1,1);

	// ��ʼ���ŵ�״̬Ϊ false
	s_grounded_map[this] = false;
	// ��ʼ����ס��Ծ������ coyote ʱ��Ϊ 0
	s_jump_hold_time_left[this] = 0.0f;
	s_coyote_time_left[this] = 0.0f;
}

void PlayerObject::Update()
{
	// 当检测到按键按下时，设置速度方向（不直接 SetPosition，使用速度积分）
    CF_V2 dir(0,0);
    if (Input::IsKeyInState(CF_KEY_A, KeyState::Hold)) {
        dir.x -= 1;
    }
    if (Input::IsKeyInState(CF_KEY_D, KeyState::Hold)) {
		dir.x += 1;
    }
    if (Input::IsKeyInState(CF_KEY_W, KeyState::Hold)) {
		dir.y += 1;
    }
    if (Input::IsKeyInState(CF_KEY_S, KeyState::Hold)) {
		dir.y -= 1;
    }

    // ��ȡ��ǰʵ�����ŵ����ʱ��״̬
    bool grounded = false;
    auto itg = s_grounded_map.find(this);
    if (itg != s_grounded_map.end()) grounded = itg->second;

    float hold_time_left = 0.0f;
    auto ith = s_jump_hold_time_left.find(this);
    if (ith != s_jump_hold_time_left.end()) hold_time_left = ith->second;

    float coyote_left = 0.0f;
    auto itc = s_coyote_time_left.find(this);
    if (itc != s_coyote_time_left.end()) coyote_left = itc->second;

    // ��Ծ������֧�� coyote time����ض�ʱ�����Կ������Լ���ס�ӳ���Ծ��ͨ������������
    static constexpr float jump_speed = 20.0f;
    bool space_down = cf_key_down(CF_KEY_SPACE);

    // ���������Ծ�������ŵػ��� coyote �����ڣ������Ұ������ڰ���״̬��������
    if (space_down && (grounded || coyote_left > 0.0f)) {
        // ���������ó��ٶȲ�������ס��������
        CF_V2 cur_vel = GetVelocity();
        cur_vel.y = jump_speed;
        SetVelocity(cur_vel);

        // ����������ŵغ� coyote ״̬��������ס��Ծ��ʱ
        s_grounded_map[this] = false;
        s_coyote_time_left[this] = 0.0f;
        s_jump_hold_time_left[this] = static_cast<float>(max_jump_hold_frames);
        hold_time_left = static_cast<float>(max_jump_hold_frames);
    }

    // �����ס�ո��һ��а�סʱ�䣬������סʱ�䣨���������ڽ���������
    if (space_down && hold_time_left > 0.0f) {
        // ÿ֡����һ֡�İ�סʱ��
        hold_time_left -= 1.0f;
        s_jump_hold_time_left[this] = hold_time_left;
    } else if (!space_down && hold_time_left > 0.0f) {
        // �ɿ�ʱȡ��ʣ�ఴסʱ��
        s_jump_hold_time_left[this] = 0.0f;
        hold_time_left = 0.0f;
    }

    // ˮƽ�ٶȣ�ֻ�޸� x������ y�����⸲����Ծ��������ɵ��ٶȣ�
    CF_V2 cur_vel = GetVelocity();
    cur_vel.x = dir.x * speed;
    SetVelocity(cur_vel);

    // ����ˮƽ�ƶ�����ת��ͼ���л�����
    if (dir.x != 0) {
		SpriteFlipX(dir.x < 0); // 根据水平移动方向翻转贴图
        SpriteSetStats("/sprites/walk.png", 2, 7, 0, false);
    }
    else {
		SpriteSetStats("/sprites/idle.png", 3, 7, 0, false);
    }

	// Ϊ Player ��������ʼ���� y �Ḻ�����������������״̬ʹ�ò�ͬ���ʣ�
    // - �ڰ�ס��Ծ�Ҵ��ڰ�ס����ʱ������������������� 0.4��������������
    // - ������ʱ���ٶ�Ϊ������ʹ�ü�ǿ�������ʣ����������ʵ�����죩
    // - �������ʹ�ñ�׼����
    static constexpr float gravity = 3.0f;        // ����ÿ֡�������ٶȣ��ɸ�����Ҫ������
    static constexpr float max_fall_speed = -12.0f; // �ն������ٶȣ���ֵ��

    // ��ȡ��ǰ��ֱ�ٶ����ж���������������
    cur_vel = GetVelocity();
    float gravity_multiplier = 1.0f;

    // ���������׶Σ���ֱ�ٶ�Ϊ�����������ڰ�ס���б���ʱ��ʱʹ�õ�����
    if (cur_vel.y > 0.0f && space_down && hold_time_left > 0.0f) {
        gravity_multiplier = low_gravity_multiplier;
    } else if (cur_vel.y < 0.0f) {
        // ����׶�ʹ�ü�ǿ����
        gravity_multiplier = fall_gravity_multiplier;
    } else {
        gravity_multiplier = 1.0f;
    }

    // Ӧ������
    AddVelocity(cf_v2(0.0f, -gravity * gravity_multiplier));

    // ���������ٶȣ���ֹ���޼��٣�
    cur_vel = GetVelocity();
    if (cur_vel.y < max_fall_speed) {
        cur_vel.y = max_fall_speed;
        SetVelocity(cur_vel);
    }

    // ÿ֡�ݼ� coyote ʱ�䣨���У�
    if (coyote_left > 0.0f) {
        coyote_left -= 1.0f;
        s_coyote_time_left[this] = coyote_left;
    }

	// 计算朝向角度（弧度制，0 度为正右，逆时针旋转）
    float angle = 0;
    if (Input::IsKeyInState(CF_KEY_Q, KeyState::Hold)) {
		angle += PI / 60.0f; // 每帧逆时针旋转 3 度
		std::cout << GetRotation() << std::endl;
    }
    if (Input::IsKeyInState(CF_KEY_E, KeyState::Hold)) {
		angle -= PI / 60.0f; // 每帧顺时针旋转 3 度
    }
	Rotate(angle);

	// 按空格键发射 TestObject 实例
    if (Input::IsKeyInState(CF_KEY_SPACE, KeyState::Down)) {
        if (objs.TryGetRegisteration(test[i])) {
			objs.Destroy(test[i]);
        }
        auto test_token = objs.Create<TestObject>();
        if (test_token.isValid()) test[i] = test_token;
        auto rot = GetRotation();
        int flip = (SpriteGetFlipX() ? -1 : 1);
        objs[test[i]].SetRotation(rot);
        objs[test[i]].SpriteFlipX(SpriteGetFlipX());
        objs[test[i]].SetPosition(GetPosition());
        objs[test[i]].SetVisible(true);
        objs[test[i]].SetVelocity(v2math::angled(CF_V2(30.0f), rot) * flip);
		i = (i + 1) % 3; // 场上仅存在3个 TestObject 实例，若多出则销毁最早生成的那个
    }
}