#include "application/ManualDriver.hpp"

#include <cmath>

namespace application
{

using domain::Car;
using domain::clampf;
using domain::DriveInput;
using domain::Track;
using domain::TrackPose;

namespace
{

constexpr float KICK_MIN_SPEED = 8.0f;  // これより遅いとドリフトに入らない（m/s）。OneButtonDriver と同じ
// キーボードのステアは押した瞬間に全開になるので、少しずつ切る（1 秒あたりの変化）
constexpr float STEER_IN_RATE = 3.5f;
constexpr float STEER_OUT_RATE = 7.0f;
// ボタンを押していないとき、後ろが滑り出したらアクセルを戻す（トラクションコントロール）
constexpr float TRACTION_SLIP = 0.12f;  // これより深く滑るとアクセルを戻し始める（ラジアン、約 7°）
constexpr float TRACTION_FULL = 0.3f;  // ここまで滑るとアクセルを全部戻す
constexpr float LINE_RATE = 7.0f;  // ドリフト中、ステアを切り切ったときにラインがずれる速さ（m/s）
constexpr float LINE_MARGIN = 4.0f;  // ラインは道の端からこれだけ内側まで（m）
constexpr float LINE_LOOK = 40.0f;  // この距離先までの、いちばん狭い道幅にラインを収める（m）

/// 周回位置 s から distance 先までで、いちばん狭い道幅の半分（m）
float narrowestAhead(const Track& track, float s, float distance)
{
    float narrowest{1e9f};
    for (const domain::TrackSample& sample : track.samples)
    {
        float ahead{std::fmod(sample.s - s + track.length, track.length)};
        if (ahead <= distance)
        {
            narrowest = std::fmin(narrowest, sample.halfWidth > 0.0f ? sample.halfWidth : track.halfWidth);
        }
    }
    return narrowest < 1e8f ? narrowest : track.halfWidth;
}

}  // namespace

DriveInput ManualDriver::drive(const Car& car, const Track& track, const PlayerControls& controls, float dt)
{
    // キーを押した瞬間に全開にせず、少しずつ切る。戻すときは速く
    float target{clampf(controls.steer, -1.0f, 1.0f)};
    float rate{std::fabs(target) > std::fabs(m_steer) && target * m_steer >= 0.0f ? STEER_IN_RATE : STEER_OUT_RATE};
    m_steer += clampf(target - m_steer, -rate * dt, rate * dt);

    const bool drifting{controls.drift && car.speed > KICK_MIN_SPEED};
    if (drifting)
    {
        // ドリフト中は、コースに沿って走る運転役に任せる。ステアは走るラインを左右へずらす
        TrackPose pose{track.query(car.x, car.z, car.y, car.ground.found ? car.ground.s : -1.0f)};
        // この先で道が狭くなるなら、先に内側へ寄せておく
        float halfWidth{std::fmin(pose.halfWidth, narrowestAhead(track, pose.s, LINE_LOOK))};
        float room{std::fmax(halfWidth - LINE_MARGIN, 0.0f)};
        if (!m_wasDrifting)
        {
            m_line = pose.lateral;
        }
        // 狭くなるときに急に寄せると振られるので、ラインの動きは LINE_RATE までにする
        float wanted{clampf(m_line + target * LINE_RATE * dt, -room, room)};
        m_line += clampf(wanted - m_line, -LINE_RATE * dt, LINE_RATE * dt);
        m_wasDrifting = true;
        return m_assist.drive(car, track, true, dt, m_line);
    }
    if (m_wasDrifting)
    {
        m_assist.reset();
        m_wasDrifting = false;
    }

    DriveInput input;
    input.throttle = clampf(controls.throttle, 0.0f, 1.0f);
    input.brake = clampf(controls.brake, 0.0f, 1.0f);
    input.steer = m_steer;
    // ボタンを押していない間は、補助が滑りを 0 へ戻す（勝手にドリフトに入らない）
    input.driftAngle = 0;
    float slip{std::fabs(car.slipAngle())};
    input.throttle *= 1.0f - clampf((slip - TRACTION_SLIP) / (TRACTION_FULL - TRACTION_SLIP), 0.0f, 1.0f);
    return input;
}

}  // namespace application
