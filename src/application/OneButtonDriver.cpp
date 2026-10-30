#include "application/OneButtonDriver.hpp"

#include <algorithm>
#include <cmath>

namespace application
{

using domain::Car;
using domain::clampf;
using domain::DriveInput;
using domain::Track;
using domain::TrackPose;
using domain::TrackSample;
using domain::wrapPi;

namespace
{

constexpr float LATERAL_GRIP = 11.0f;     // 曲がるときに出してよい横向きの加速度（m/s²）
constexpr float MIN_SPEED = 13.0f;        // 自動で走る最低の速度（m/s、約 47 km/h）
constexpr float MAX_SPEED = 55.0f;        // 自動で走る最高の速度（m/s、約 200 km/h）
constexpr float SPEED_LOOK = 70.0f;       // この距離先までのカーブを見て速度を決める（m）
constexpr float KICK_TIME = 0.18f;        // ボタンを押した直後にサイドブレーキを引く時間（秒）
constexpr float KICK_MIN_SPEED = 8.0f;    // これより遅いとドリフトに入らない（m/s）
constexpr float DRIFT_MIN_ANGLE = 0.32f;  // ドリフトの角度の下限（ラジアン、約 18°）
constexpr float DRIFT_MAX_ANGLE = 0.78f;  // ドリフトの角度の上限（約 45°）
constexpr float DRIFT_ANGLE_PER_CURVE = 14.0f;  // カーブのきつさ（1/m）あたりに足す角度
constexpr float DRIFT_GRIP = 7.0f;  // ドリフト中に出してよい横向きの加速度（m/s²）。滑る分だけ低め
constexpr float STEER_GAIN = 1.35f;  // 狙った曲がり具合に対して、前輪を切る強さ
// ドリフト中は外へ膨らみやすいので、近くを見て強めに切る
constexpr float DRIFT_STEER_GAIN = 2.0f;
constexpr float DRIFT_LOOK_SCALE = 0.75f;
// 先のカーブがこれよりきつく逆向きなら切り返す（1/m。半径 60m）
constexpr float SWITCH_CURVATURE = 1.0f / 60.0f;

/// 周回位置 s の中心線の点（隣の点の間をつなぐ）
void pointAt(const Track& track, float s, float& x, float& z)
{
    const auto& samples{track.samples};
    const int n{static_cast<int>(samples.size())};
    s = std::fmod(std::fmod(s, track.length) + track.length, track.length);
    // 点はおおむね等間隔なので、近くから探す
    int i{static_cast<int>(s / track.length * float(n))};
    i = std::clamp(i, 0, n - 1);
    while (i > 0 && samples[std::size_t(i)].s > s)
    {
        --i;
    }
    while (i < n - 1 && samples[std::size_t(i + 1)].s <= s)
    {
        ++i;
    }
    const TrackSample& a{samples[std::size_t(i)]};
    const TrackSample& b{samples[std::size_t((i + 1) % n)]};
    float span{(i + 1 < n ? b.s : track.length) - a.s};
    float t{span > 1e-4f ? (s - a.s) / span : 0.0f};
    x = a.x + (b.x - a.x) * t;
    z = a.z + (b.z - a.z) * t;
}

/// 周回位置 s での中心線の向き
float headingAt(const Track& track, float s)
{
    float x0{};
    float z0{};
    float x1{};
    float z1{};
    pointAt(track, s - 2.0f, x0, z0);
    pointAt(track, s + 2.0f, x1, z1);
    return std::atan2(x1 - x0, z1 - z0);
}

/// s から先の範囲のカーブのきつさ（1/m）。右が正。いちばんきつい所を返す
float curvatureAhead(const Track& track, float s, float from, float to)
{
    float strongest{};
    for (float d{from}; d < to; d += 5.0f)
    {
        float k{wrapPi(headingAt(track, s + d + 5.0f) - headingAt(track, s + d)) / 5.0f};
        if (std::fabs(k) > std::fabs(strongest))
        {
            strongest = k;
        }
    }
    return strongest;
}

}  // namespace

DriveInput OneButtonDriver::drive(const Car& car, const Track& track, bool driftButton, float dt, float lineOffset)
{
    DriveInput input;
    if (track.samples.size() < 3)
    {
        return input;
    }
    TrackPose pose{track.query(car.x, car.z, car.y, car.ground.found ? car.ground.s : -1.0f)};
    float speed{std::fmax(car.speed, 0.0f)};

    // ---- ステア: 進む向き（車体の向き＋滑り角）を、少し先の中心線（から lineOffset ずらした点）へ向ける ----
    const bool drifting{driftButton && speed > KICK_MIN_SPEED};
    float lookahead{clampf(8.0f + 0.6f * speed, 10.0f, 32.0f) * (drifting ? DRIFT_LOOK_SCALE : 1.0f)};
    float tx{};
    float tz{};
    pointAt(track, pose.s + lookahead, tx, tz);
    if (lineOffset != 0.0f)
    {
        float heading{headingAt(track, pose.s + lookahead)};
        tx += std::cos(heading) * lineOffset;
        tz -= std::sin(heading) * lineOffset;
    }
    float travel{car.yaw + car.slipAngle()};
    float alpha{wrapPi(std::atan2(tx - car.x, tz - car.z) - travel)};
    float curvature{2.0f * std::sin(alpha) / lookahead};
    float wheel{car.slipAngle() + std::atan(3.05f * curvature) * (drifting ? DRIFT_STEER_GAIN : STEER_GAIN)};
    input.steer = car.steerInputFor(wheel);

    // ---- アクセルとブレーキ: この先のカーブのきつさから、出してよい速度を決める ----
    float ahead{curvatureAhead(track, pose.s, 0.0f, SPEED_LOOK)};
    float target{MAX_SPEED};
    if (std::fabs(ahead) > 1e-4f)
    {
        target = clampf(std::sqrt(LATERAL_GRIP / std::fabs(ahead)), MIN_SPEED, MAX_SPEED);
    }
    input.throttle = clampf((target - speed) * 0.4f + 0.35f, 0.0f, 1.0f);
    if (speed > target + 2.0f)
    {
        input.throttle = 0;
        input.brake = clampf((speed - target) * 0.12f, 0.0f, 1.0f);
    }

    // ---- ドリフト: ボタンを押している間だけ ----
    input.driftAngle = 0;
    if (driftButton && speed > KICK_MIN_SPEED)
    {
        if (!m_wasHeld)
        {
            // 押した瞬間に、これから曲がる向きを決める。まっすぐなら、今向かっている向き
            float turn{curvatureAhead(track, pose.s, 0.0f, 40.0f)};
            m_direction = std::fabs(turn) > 0.002f ? (turn > 0 ? 1.0f : -1.0f) : (alpha >= 0 ? 1.0f : -1.0f);
            m_holdTime = 0;
        }
        else
        {
            // 押し続けている間に先のカーブが逆向きになったら、自動で切り返す（S 字で倍率が上がる）
            float next{curvatureAhead(track, pose.s, 5.0f, 25.0f)};
            float sign{next > 0 ? 1.0f : -1.0f};
            if (std::fabs(next) > SWITCH_CURVATURE && sign != m_direction)
            {
                m_direction = sign;
            }
        }
        m_holdTime += dt;
        // ドリフト中も速度は見張る。速すぎるとカーブの外へ膨らんで壁に当たる
        float driftTarget{MAX_SPEED};
        if (std::fabs(ahead) > 1e-4f)
        {
            driftTarget = clampf(std::sqrt(DRIFT_GRIP / std::fabs(ahead)), MIN_SPEED, MAX_SPEED);
        }
        input.brake = 0;
        input.throttle = clampf((driftTarget - speed) * 0.5f + 0.6f, 0.2f, 1.0f);
        if (speed > driftTarget + 3.0f)
        {
            input.throttle = 0.1f;
            input.brake = clampf((speed - driftTarget) * 0.1f, 0.0f, 0.6f);
        }
        if (m_holdTime < KICK_TIME)
        {
            // 曲がる向きへ切りながらサイドブレーキで後ろを振り出す
            input.handbrake = true;
            input.steer = clampf(input.steer + m_direction * 0.6f, -1.0f, 1.0f);
        }
        float local{std::fabs(curvatureAhead(track, pose.s, 0.0f, 25.0f))};
        input.driftAngle = clampf(DRIFT_MIN_ANGLE + DRIFT_ANGLE_PER_CURVE * local, DRIFT_MIN_ANGLE, DRIFT_MAX_ANGLE);
        input.driftDirection = m_direction;
    }
    m_wasHeld = driftButton && speed > KICK_MIN_SPEED;
    return input;
}

}  // namespace application
