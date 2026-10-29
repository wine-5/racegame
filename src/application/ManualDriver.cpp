#include "application/ManualDriver.hpp"

#include <cmath>

namespace application
{

using domain::Car;
using domain::clampf;
using domain::DriveInput;

namespace
{

constexpr float KICK_TIME = 0.18f;      // ボタンを押した直後にサイドブレーキを引く時間（秒）
constexpr float KICK_STEER = 0.6f;      // 振り出すときに曲がる向きへ足すステア
constexpr float KICK_MIN_SPEED = 8.0f;  // これより遅いとドリフトに入らない（m/s）
constexpr float DRIFT_BASE_ANGLE = 0.52f;  // ステアを切っていないときのドリフトの角度（ラジアン、約 30°）
constexpr float DRIFT_STEER_ANGLE = 0.26f;  // ステアを切り切ったときに増減する角度
constexpr float DRIFT_MIN_ANGLE = 0.3f;
constexpr float DRIFT_MAX_ANGLE = 0.8f;
constexpr float DRIFT_MIN_THROTTLE = 0.55f;  // ドリフト中はアクセルを離していても少し踏んで滑りを保つ
constexpr float SWITCH_STEER = 0.5f;  // 曲がる向きと逆へこれ以上切ると切り返しの準備
constexpr float SWITCH_TIME = 0.12f;  // 逆へ切り続けてから切り返すまでの時間（秒）

float signOf(float value)
{
    return value >= 0.0f ? 1.0f : -1.0f;
}

}  // namespace

DriveInput ManualDriver::drive(const Car& car, const PlayerControls& controls, float dt)
{
    DriveInput input;
    input.throttle = clampf(controls.throttle, 0.0f, 1.0f);
    input.brake = clampf(controls.brake, 0.0f, 1.0f);
    input.steer = clampf(controls.steer, -1.0f, 1.0f);

    const bool drifting{controls.drift && car.speed > KICK_MIN_SPEED};
    if (drifting)
    {
        if (!m_wasHeld)
        {
            // 押した瞬間に曲がる向きを決める。ステアを切っていなければ、今回っている向き
            if (std::fabs(input.steer) > 0.1f)
            {
                m_direction = signOf(input.steer);
            }
            else
            {
                m_direction = std::fabs(car.yawRate) > 0.05f ? signOf(car.yawRate) : m_direction;
            }
            m_holdTime = 0;
            m_counterTime = 0;
        }
        else
        {
            // 逆へ切り続けたら切り返す
            m_counterTime = input.steer * m_direction < -SWITCH_STEER ? m_counterTime + dt : 0.0f;
            if (m_counterTime > SWITCH_TIME)
            {
                m_direction = -m_direction;
                m_counterTime = 0;
            }
        }
        m_holdTime += dt;

        if (m_holdTime < KICK_TIME)
        {
            input.handbrake = true;
            input.steer = clampf(input.steer + m_direction * KICK_STEER, -1.0f, 1.0f);
        }
        float steerInto{clampf(input.steer * m_direction, -1.0f, 1.0f)};
        input.driftAngle = clampf(DRIFT_BASE_ANGLE + DRIFT_STEER_ANGLE * steerInto, DRIFT_MIN_ANGLE, DRIFT_MAX_ANGLE);
        input.driftDirection = m_direction;
        input.throttle = std::fmax(input.throttle, DRIFT_MIN_THROTTLE);
        input.brake = 0;
    }
    m_wasHeld = drifting;
    return input;
}

}  // namespace application
