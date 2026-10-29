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

float maxSteerAt(float speed)
{
    return 0.46f / (1.0f + std::fabs(speed) * 0.085f);
}

}  // namespace

void Car::reset(const Track& track)
{
    x = track.spawnX;
    y = track.spawnY;
    z = track.spawnZ;
    yaw = track.spawnYaw;
    speed = 0;
    steerAngle = 0;
    wheelSpin = 0;
    brakeHold = 0;
    onAsphalt = true;
    ground = track.query(x, z, y);
}

void Car::stepGrip(float dt)
{
    // ------------------------------------------------------------------------
    // グリップ走行。ドリフトはここには書いていない。
    //
    // 速度は車体の前方だけを向く。横滑りはしない。
    // 走っているときの曲がりは、滑らない自転車モデル（ホイールベースと前輪の切れ角）。
    // ほぼ止まっているときは、据え切りで少しだけ向きが変わる。壁に向かって止まっても戻れる。
    //
    // ドリフトの課題では、この関数を差し替える。
    // たとえば横方向の速度、スリップ角、前後で違うグリップをここに足す。
    // アクセル、ブレーキ、壁との接触は update() 側にある。
    // ------------------------------------------------------------------------
    float yawRate{0};
    if (std::fabs(speed) > 0.05f)
    {
        yawRate = (speed / WHEELBASE) * std::tan(steerAngle);
    }
    float slow{clampf(1.0f - std::fabs(speed) / 8.0f, 0.0f, 1.0f)};
    yawRate += steerAngle * 0.9f * slow;
    yaw += yawRate * dt;

    x += std::sin(yaw) * speed * dt;
    z += std::cos(yaw) * speed * dt;
}

void Car::update(float dt, const DriveInput& input, const Track& track)
{
    float target{clampf(input.steer, -1.0f, 1.0f) * maxSteerAt(speed)};
    float step{STEER_RATE * dt};
    steerAngle += clampf(target - steerAngle, -step, step);
    steerAngle = clampf(steerAngle, -0.6f, 0.6f);

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
        force += input.throttle * ACCEL * (1.0f - ratio * ratio);
    }
    force -= DRAG * speed * std::fabs(speed);
    if (std::fabs(speed) > 0.05f)
    {
        force -= ROLLING * (speed > 0 ? 1.0f : -1.0f);
    }
    // 芝は加速が鈍く、速度も伸びない。横には滑らない。
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

    stepGrip(dt);

    float preferS{ground.found ? ground.s : -1.0f};
    ground = track.query(x, z, y, preferS);
    if (!ground.found)
    {
        return;
    }

    const float wall{track.wallLateral()};
    if (std::fabs(ground.lateral) > wall)
    {
        float sign{ground.lateral > 0 ? 1.0f : -1.0f};
        float overflow{std::fabs(ground.lateral) - wall};
        x -= ground.rightX * sign * overflow;
        z -= ground.rightZ * sign * overflow;
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

    float bankLimit{track.halfWidth + track.curbWidth};
    float bankLat{clampf(ground.lateral, -bankLimit, bankLimit)};
    y = ground.centerY + bankLat * ground.cross;
    onAsphalt = std::fabs(ground.lateral) <= track.halfWidth + 0.15f;
}

}  // namespace domain
