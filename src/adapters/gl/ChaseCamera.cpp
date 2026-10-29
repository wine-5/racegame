#include "adapters/gl/ChaseCamera.hpp"

#include <cmath>

namespace adapters::gl
{

using domain::TrackPose;
using domain::Vec3;

namespace
{

// ドリフト中に、カメラを車体の向きから進む向きへどれだけ寄せるか（1 で進む向きそのもの）。
// 寄せると、車体が斜めに滑っている角度が画面で見える
constexpr float VELOCITY_FOLLOW = 0.7f;

// 速いほど視野角を広げる。周りが速く流れて見え、速度を感じやすくなる
constexpr float BASE_FOV = 60.0f;
constexpr float SPEED_FOV = 14.0f;       // 最大でこれだけ広げる（度）
constexpr float SPEED_FOV_FROM = 15.0f;  // この速度から広げ始める（m/s、約 54 km/h）
constexpr float SPEED_FOV_TO = 70.0f;    // この速度で最大になる（m/s、約 250 km/h）
// ドリフトが深いほど、さらに少し広げる
constexpr float DRIFT_FOV = 6.0f;
constexpr float DRIFT_FOV_ANGLE = 0.8f;  // この角度で最大になる（ラジアン、約 46°）
// 視野角が目標に追いつく速さ。急に変わると酔うので、位置より遅くする
constexpr float FOV_RESPONSE = 2.5f;
// 揺れの強さ 1 のときに、カメラが動く距離（m）と、揺れが収まる速さ
constexpr float SHAKE_DISTANCE = 0.45f;
constexpr float SHAKE_DECAY = 7.0f;
// ドリフト中の細かい揺れ（滑り角 1 ラジアンにつき）
constexpr float RUMBLE = 0.12f;
// カメラが壁からこれだけ内側にいるようにする（m）
constexpr float WALL_MARGIN = 0.6f;

}  // namespace

void ChaseCamera::update(const domain::Car& car, const domain::Track& track, float dt)
{
    const float back[MODE_COUNT]{8.6f, 4.1f, 16.0f};
    const float up[MODE_COUNT]{3.3f, 1.55f, 7.2f};
    const float ahead[MODE_COUNT]{9.0f, 6.0f, 14.0f};
    float viewYaw{car.yaw + car.slipAngle() * VELOCITY_FOLLOW};
    Vec3 forward{std::sin(viewYaw), 0, std::cos(viewYaw)};
    Vec3 position{car.x, car.y, car.z};
    Vec3 eye{position - forward * back[m_mode] + Vec3{0, up[m_mode], 0}};
    Vec3 look{position + forward * ahead[m_mode] + Vec3{0, 1.15f, 0}};
    // 路面より下に潜らない
    TrackPose under{track.query(eye.x, eye.z, car.y, car.ground.s)};
    if (under.found && under.xzDist < 30.0f && eye.y < under.centerY + 0.8f)
    {
        eye.y = under.centerY + 0.8f;
    }
    float speedAmount{std::fmin(std::fmax((car.speed - SPEED_FOV_FROM) / (SPEED_FOV_TO - SPEED_FOV_FROM), 0.0f), 1.0f)};
    float driftAmount{std::fmin(std::fabs(car.slipAngle()) / DRIFT_FOV_ANGLE, 1.0f)};
    float fov{BASE_FOV + SPEED_FOV * speedAmount + DRIFT_FOV * driftAmount};
    if (!m_ready)
    {
        m_eye = eye;
        m_look = look;
        m_fovDegrees = fov;
        m_ready = true;
        return;
    }
    m_fovDegrees += (fov - m_fovDegrees) * (1.0f - std::exp(-dt * FOV_RESPONSE));
    float blend{1.0f - std::exp(-dt * 7.0f)};
    m_eye = m_eye + (eye - m_eye) * blend;
    m_look = m_look + (look - m_look) * blend;
    keepInside(track, car);

    // 揺れ。深く速く滑っている間は、路面のざらつきのように細かく揺らし続ける
    float rumble{std::fabs(car.slipAngle()) * std::fmin(car.speed / 30.0f, 1.0f) * RUMBLE};
    float amount{std::fmax(m_shake, rumble)};
    m_shakeTime += dt;
    m_shakeOffset = Vec3{std::sin(m_shakeTime * 37.0f), std::sin(m_shakeTime * 43.0f + 1.3f) * 0.7f,
                         std::sin(m_shakeTime * 31.0f + 2.1f)} *
                    (amount * SHAKE_DISTANCE);
    m_shake *= std::exp(-dt * SHAKE_DECAY);
}

void ChaseCamera::keepInside(const domain::Track& track, const domain::Car& car)
{
    // 壁（コンテナなど）の向こうへカメラが出ると、画面が壁の中になる。壁の少し内側へ戻す
    TrackPose at{track.query(m_eye.x, m_eye.z, car.y, car.ground.s)};
    if (!at.found)
    {
        return;
    }
    float limit{track.wallLateral(at.halfWidth) - WALL_MARGIN};
    float over{std::fabs(at.lateral) - limit};
    if (over > 0)
    {
        float sign{at.lateral > 0 ? 1.0f : -1.0f};
        m_eye.x -= at.rightX * sign * over;
        m_eye.z -= at.rightZ * sign * over;
    }
}

const char* ChaseCamera::modeName() const
{
    const char* names[MODE_COUNT]{"CHASE", "CLOSE", "FAR"};
    return names[m_mode];
}

}  // namespace adapters::gl
