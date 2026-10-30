#include "domain/Car.hpp"

#include <cmath>

namespace domain
{

namespace
{

constexpr float MAX_SPEED = 82.0f;  // 約 295 km/h
constexpr float MAX_REVERSE = 9.0f;
constexpr float ACCEL = 13.0f;
constexpr float BRAKE = 30.0f;
constexpr float REVERSE_ACCEL = 8.0f;
constexpr float DRAG = 0.00045f;
constexpr float ROLLING = 0.9f;
constexpr float WHEELBASE = 3.05f;
constexpr float STEER_RATE = 2.6f;
constexpr float WHEEL_RADIUS = 0.34f;
constexpr float MAX_LOCK = 0.6f;         // 前輪の切れ角の上限（ラジアン）
constexpr float MAX_DRIFT_LOCK = 0.9f;   // ドリフト中の切れ角の上限（約 52°）
constexpr float DRIFT_LOCK_GAIN = 0.5f;  // 滑り角 1 ラジアンにつき、切れ角の上限を広げる量

// ---- タイヤと車体 ----
constexpr float GRAVITY = 9.81f;
constexpr float MASS = 1200.0f;         // kg
constexpr float YAW_INERTIA = 1700.0f;  // kg·m²
constexpr float CG_TO_FRONT = 1.45f;    // 重心から前輪まで（m）
constexpr float CG_TO_REAR = WHEELBASE - CG_TO_FRONT;
constexpr float GRIP_ASPHALT =
    1.3f;  // 摩擦係数。アーケード向けに実車より高め  // 摩擦係数。アーケード向けに実車より高め  //
           // 摩擦係数。アーケード向けに実車より高め  // 摩擦係数。アーケード向けに実車より高め  //
           // 摩擦係数。アーケード向けに実車より高め  // 摩擦係数
constexpr float GRIP_GRASS = 0.6f;
// Pacejka の簡易式 sin(C·atan(B·α))。限界を超えると力が少し落ちるので、滑りが続く
constexpr float TIRE_B = 9.0f;
constexpr float TIRE_C = 1.35f;
constexpr int TIRE_SUBSTEPS = 4;

// 後輪のグリップを前輪より少し強くして、普通に曲がる分には限界でも前から滑る（安定する）ようにする
constexpr float REAR_GRIP_BIAS = 1.2f;

// ---- ドリフトのきっかけ ----
// アクセルを全開にすると、後輪の横向きのグリップがこの割合まで減る
constexpr float THROTTLE_REAR_GRIP = 0.62f;
constexpr float HANDBRAKE_REAR_GRIP = 0.35f;
constexpr float HANDBRAKE_DECEL = 5.0f;  // m/s²
// 横に滑っているとき、アクセルの力がこの割合まで失われる（真横に滑ったとき）
constexpr float DRIFT_DRIVE_LOSS = 0.15f;
// ドリフトを頼まれている間の後輪のグリップ（前輪に対する割合）
constexpr float REQUEST_REAR_GRIP = 0.62f;

// ---- アーケード向けの補助 ----
// 滑っている向きへ自動で当てるカウンターステアの強さ
constexpr float COUNTER_STEER_ASSIST = 0.75f;
// ドリフト中は、入力で決めた目標の角度へ車体の回転を寄せる。キーボードでも角度を保てる
constexpr float DRIFT_DETECT_MIN = 0.10f;   // この角度から補助を効かせ始める（ラジアン、約 6°）
constexpr float DRIFT_DETECT_FULL = 0.25f;  // この角度で補助が最大になる（約 14°）
constexpr float DRIFT_BASE_ANGLE = 0.50f;  // ステアを離してアクセルを踏んでいるときの角度（約 29°）
constexpr float DRIFT_STEER_ANGLE = 0.30f;  // 曲がる向きへ切ると深く、逆へ切ると浅くなる量
constexpr float DRIFT_MIN_ANGLE = 0.08f;
constexpr float DRIFT_MAX_ANGLE = 0.90f;
constexpr float DRIFT_ASSIST_P = 26.0f;  // 角度のずれに対する強さ
constexpr float DRIFT_ASSIST_D = 5.0f;   // 角度の変わる速さに対するブレーキ
// アクセルをこれより戻すと、目標の角度が小さくなってドリフトが終わる
constexpr float DRIFT_THROTTLE_HOLD = 0.35f;

// これより深い角度になると、元に戻す力をかける（ラジアン、約 55°）
constexpr float MAX_SLIP_ANGLE = 0.96f;
constexpr float SLIP_LIMIT_GAIN = 9.0f;

// これより遅いと滑らない動きに切り替える（m/s）
constexpr float TIRE_MIN_SPEED = 2.0f;
constexpr float TIRE_FULL_SPEED = 5.0f;

float maxSteerAt(float speed)
{
    return 0.46f / (1.0f + std::fabs(speed) * 0.085f);
}

/// 滑り角 alpha のときの横向きの力の割合（-1..1）
float tireForce(float alpha)
{
    return std::sin(TIRE_C * std::atan(TIRE_B * alpha));
}

/// 0 なら滑らない動き、1 ならタイヤの力で動く
float tireBlend(float speed)
{
    return clampf((speed - TIRE_MIN_SPEED) / (TIRE_FULL_SPEED - TIRE_MIN_SPEED), 0.0f, 1.0f);
}

}  // namespace

void Car::reset(const Track& track)
{
    x = track.spawnX;
    y = track.spawnY;
    z = track.spawnZ;
    yaw = track.spawnYaw;
    speed = 0;
    lateralSpeed = 0;
    yawRate = 0;
    steerAngle = 0;
    wheelSpin = 0;
    brakeHold = 0;
    onAsphalt = true;
    hitWall = false;
    ground = track.query(x, z, y);
}

void Car::stepGrip(float dt)
{
    // 速度は車体の前方だけを向く。横滑りはしない。
    // 走っているときの曲がりは、滑らない自転車モデル（ホイールベースと前輪の切れ角）。
    // ほぼ止まっているときは、据え切りで少しだけ向きが変わる。壁に向かって止まっても戻れる。
    float rate{0};
    if (std::fabs(speed) > 0.05f)
    {
        rate = (speed / WHEELBASE) * std::tan(steerAngle);
    }
    float slow{clampf(1.0f - std::fabs(speed) / 8.0f, 0.0f, 1.0f)};
    rate += steerAngle * 0.9f * slow;
    yaw += rate * dt;
    yawRate = rate;
    lateralSpeed = 0;

    x += std::sin(yaw) * speed * dt;
    z += std::cos(yaw) * speed * dt;
}

float Car::maxLock() const
{
    // ドリフト中は前輪を進む向きへそろえられるよう、深く切れるようにする
    return std::fmin(MAX_LOCK + std::fabs(slipAngle()) * DRIFT_LOCK_GAIN, MAX_DRIFT_LOCK);
}

float Car::steerLimit() const
{
    return std::fmin(maxSteerAt(speed) + std::fabs(slipAngle()), maxLock());
}

float Car::steerInputFor(float wheelAngle) const
{
    float limit{steerLimit()};
    if (limit < 1e-4f)
    {
        return 0;
    }
    return clampf((wheelAngle - slipAngle() * COUNTER_STEER_ASSIST) / limit, -1.0f, 1.0f);
}

float Car::slipAngle() const
{
    if (std::fabs(speed) < TIRE_MIN_SPEED)
    {
        return 0;
    }
    return std::atan2(lateralSpeed, std::fabs(speed));
}

void Car::stepTire(float dt, const DriveInput& input)
{
    // ------------------------------------------------------------------------
    // 前後 2 輪の自転車モデル。車体の座標で、前向きの速度 u、横向きの速度 v（右が正）、
    // 回転の速さ r（右回りが正）を進める。
    //
    //   前輪の滑り角 αf = atan((v + a·r) / u) - 切れ角
    //   後輪の滑り角 αr = atan((v - b·r) / u)
    //   横向きの力   F = -μ·N·sin(C·atan(B·α))
    //
    // 後輪のグリップは、アクセルとサイドブレーキで減る。後輪だけが限界を超えると
    // 車体の後ろが外へ流れ、ドリフトになる。
    // ------------------------------------------------------------------------
    const float mu{onAsphalt ? GRIP_ASPHALT : GRIP_GRASS};
    const float frontLoad{MASS * GRAVITY * CG_TO_REAR / WHEELBASE};
    const float rearLoad{MASS * GRAVITY * CG_TO_FRONT / WHEELBASE};
    float rearGrip{REAR_GRIP_BIAS * (1.0f - (1.0f - THROTTLE_REAR_GRIP) * input.throttle)};
    if (input.handbrake)
    {
        rearGrip = std::fmin(rearGrip, HANDBRAKE_REAR_GRIP);
    }
    // ドリフトを頼まれている間は、後輪を滑りやすくして車の後ろを流す
    const bool requested{input.driftAngle > 0.0f && input.driftDirection != 0.0f};
    if (requested)
    {
        rearGrip = std::fmin(rearGrip, REQUEST_REAR_GRIP);
    }

    const float h{dt / float(TIRE_SUBSTEPS)};
    float prevSlip{std::atan2(lateralSpeed, std::fmax(speed, 1.0f))};
    for (int i{}; i < TIRE_SUBSTEPS; ++i)
    {
        float u{std::fmax(speed, 1.0f)};
        float alphaFront{std::atan2(lateralSpeed + CG_TO_FRONT * yawRate, u) - steerAngle};
        float alphaRear{std::atan2(lateralSpeed - CG_TO_REAR * yawRate, u)};
        float forceFront{-mu * frontLoad * tireForce(alphaFront)};
        float forceRear{-mu * rearLoad * rearGrip * tireForce(alphaRear)};

        float cosSteer{std::cos(steerAngle)};
        float lateralAccel{(forceFront * cosSteer + forceRear) / MASS - speed * yawRate};
        float yawAccel{(CG_TO_FRONT * forceFront * cosSteer - CG_TO_REAR * forceRear) / YAW_INERTIA};
        // 前輪の横向きの力のうち、進む向きと逆の成分で速度が落ちる。曲がっている向きへの回転で
        // 横向きの速度が前向きに回り込む
        float forwardAccel{lateralSpeed * yawRate - forceFront * std::sin(steerAngle) / MASS};

        float slip{std::atan2(lateralSpeed, u)};
        float slipRate{(slip - prevSlip) / h};
        prevSlip = slip;

        // アーケード向けの補助: ドリフト中は、入力で決めた角度へ寄せる
        float drifting{
            clampf((std::fabs(slip) - DRIFT_DETECT_MIN) / (DRIFT_DETECT_FULL - DRIFT_DETECT_MIN), 0.0f, 1.0f)};
        if (requested)
        {
            // 頼まれたときは、まだ滑っていなくても目標の角度へ回し始める
            drifting = 1.0f;
        }
        if (drifting > 0.0f && !input.handbrake)
        {
            // 右へ曲がるドリフトでは進む向きが車体の左になるので slip は負
            float direction{requested ? -input.driftDirection : (slip < 0 ? -1.0f : 1.0f)};
            float target{direction * input.driftAngle};
            if (input.driftAngle < 0.0f)
            {
                float steerInto{-direction * input.steer};
                float hold{clampf(input.throttle / DRIFT_THROTTLE_HOLD, 0.0f, 1.0f)};
                float angle{clampf(DRIFT_BASE_ANGLE + DRIFT_STEER_ANGLE * steerInto, DRIFT_MIN_ANGLE, DRIFT_MAX_ANGLE)};
                target = direction * (DRIFT_MIN_ANGLE + (angle - DRIFT_MIN_ANGLE) * hold);
            }
            yawAccel += drifting * (DRIFT_ASSIST_P * (slip - target) + DRIFT_ASSIST_D * slipRate);
        }

        // 角度が深くなりすぎたら車体を進む向きへ戻す
        if (std::fabs(slip) > MAX_SLIP_ANGLE)
        {
            float over{slip - (slip > 0 ? MAX_SLIP_ANGLE : -MAX_SLIP_ANGLE)};
            yawAccel += over * SLIP_LIMIT_GAIN * u;
        }

        lateralSpeed += lateralAccel * h;
        yawRate += yawAccel * h;
        speed += forwardAccel * h;

        yaw += yawRate * h;
        x += (std::sin(yaw) * speed + std::cos(yaw) * lateralSpeed) * h;
        z += (std::cos(yaw) * speed - std::sin(yaw) * lateralSpeed) * h;
    }
}

void Car::update(float dt, const DriveInput& input, const Track& track)
{
    // 滑っているときは、その分だけ大きく切れるようにし、滑る向きへ自動で少し当てる
    float slip{slipAngle()};
    float target{clampf(input.steer, -1.0f, 1.0f) * steerLimit() + slip * COUNTER_STEER_ASSIST};
    float lock{maxLock()};
    float step{STEER_RATE * dt};
    steerAngle += clampf(target - steerAngle, -step, step);
    steerAngle = clampf(steerAngle, -lock, lock);

    if (std::fabs(speed) < 0.45f && input.brake > 0.5f && input.throttle < 0.1f)
    {
        brakeHold += dt;
    }
    else
    {
        brakeHold = 0;
    }
    bool reversing{brakeHold > 0.28f};

    float force{0};
    float speedBefore{speed};
    if (input.brake > 0.05f && !reversing)
    {
        force -= input.brake * BRAKE * (speed >= 0 ? 1.0f : -1.0f);
    }
    else if (reversing)
    {
        force -= input.brake * REVERSE_ACCEL;
    }
    else if (input.throttle > 0.0f && speed > -0.4f)
    {
        float ratio{clampf(speed / MAX_SPEED, 0.0f, 1.0f)};
        // 横に滑っている分、後輪が空転して前へ進む力が減る
        float sliding{std::fabs(std::sin(slip))};
        force += input.throttle * ACCEL * (1.0f - ratio * ratio) * (1.0f - DRIFT_DRIVE_LOSS * sliding);
    }
    if (input.handbrake && std::fabs(speed) > 0.05f)
    {
        force -= HANDBRAKE_DECEL * (speed > 0 ? 1.0f : -1.0f);
    }
    force -= DRAG * speed * std::fabs(speed);
    if (std::fabs(speed) > 0.05f)
    {
        force -= ROLLING * (speed > 0 ? 1.0f : -1.0f);
    }
    // 芝は加速が鈍く、速度も伸びない。
    if (!onAsphalt)
    {
        if (force > 0)
        {
            force *= 0.55f;
        }
        if (std::fabs(speed) > 0.2f)
        {
            force -= 3.2f * (speed > 0 ? 1.0f : -1.0f);
        }
    }

    speed += force * dt;
    if (!reversing && input.brake > 0.05f)
    {
        if (speedBefore >= 0 && speed < 0)
            speed = 0;
        if (speedBefore <= 0 && speed > 0)
            speed = 0;
    }
    speed = clampf(speed, -MAX_REVERSE, MAX_SPEED);
    wheelSpin += speed / WHEEL_RADIUS * dt;

    float blend{tireBlend(speed)};
    if (blend <= 0.0f)
    {
        stepGrip(dt);
    }
    else
    {
        stepTire(dt, input);
        if (blend < 1.0f)
        {
            // 遅くなってきたら滑りを収め、滑らない動きへなめらかにつなぐ
            float settle{1.0f - blend};
            lateralSpeed -= lateralSpeed * settle * std::fmin(1.0f, 8.0f * dt);
            float gripRate{(speed / WHEELBASE) * std::tan(steerAngle)};
            yawRate += (gripRate - yawRate) * settle * std::fmin(1.0f, 8.0f * dt);
        }
    }

    float preferS{ground.found ? ground.s : -1.0f};
    ground = track.query(x, z, y, preferS);
    if (!ground.found)
    {
        return;
    }

    hitWall = false;
    const float wall{track.wallLateral(ground.halfWidth)};
    if (std::fabs(ground.lateral) > wall)
    {
        hitWall = true;
        float sign{ground.lateral > 0 ? 1.0f : -1.0f};
        float overflow{std::fabs(ground.lateral) - wall};
        x -= ground.rightX * sign * overflow;
        z -= ground.rightZ * sign * overflow;
        // 壁の外へ向かう速度を消す。横滑りしたまま当たっても壁を抜けない
        float worldVx{std::sin(yaw) * speed + std::cos(yaw) * lateralSpeed};
        float worldVz{std::cos(yaw) * speed - std::sin(yaw) * lateralSpeed};
        float outward{sign * (worldVx * ground.rightX + worldVz * ground.rightZ)};
        if (outward > 0)
        {
            worldVx -= sign * ground.rightX * outward;
            worldVz -= sign * ground.rightZ * outward;
            speed = std::sin(yaw) * worldVx + std::cos(yaw) * worldVz;
            lateralSpeed = std::cos(yaw) * worldVx - std::sin(yaw) * worldVz;
            // 回転を少しだけ弱める。弱めすぎると、壁から離れる向きへ曲がれなくなる
            yawRate *= 0.85f;
        }
        // 壁の外へ向いている分だけ速度を落とす。コースに沿っているときは壁を擦るだけ。
        float facingOut{sign * (std::sin(yaw) * ground.rightX + std::cos(yaw) * ground.rightZ)};
        if (facingOut > 0.15f)
        {
            float cap{2.0f + 8.0f * (1.0f - facingOut)};
            if (speed > cap)
            {
                speed = cap;
            }
        }
        ground.lateral = sign * wall;
    }

    float bankLimit{ground.halfWidth + track.curbWidth};
    float bankLat{clampf(ground.lateral, -bankLimit, bankLimit)};
    y = ground.centerY + bankLat * ground.cross;
    onAsphalt = std::fabs(ground.lateral) <= ground.halfWidth + 0.15f;
}

}  // namespace domain
