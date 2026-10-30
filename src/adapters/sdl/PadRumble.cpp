#include "adapters/sdl/PadRumble.hpp"

#include <algorithm>
#include <cmath>

namespace adapters::sdl
{

namespace
{

constexpr float SEND_INTERVAL = 0.1f;  // ドリフト中の揺れを送る間隔（秒）。毎フレーム送らない
constexpr int SEND_DURATION_MS = 140;  // 1 回に送る揺れの長さ。間隔より少し長くして途切れないようにする

}  // namespace

void PadRumble::pulse(float strong, float weak, float seconds)
{
    float strength{std::max(strong, weak)};
    if (m_pulseLeft > 0.0f && strength < m_pulseStrength)
    {
        return;
    }
    m_input.rumble(strong, weak, static_cast<int>(seconds * 1000.0f));
    m_pulseLeft = seconds;
    m_pulseStrength = strength;
}

void PadRumble::update(const application::GameSession& session, float dt)
{
    const domain::Car& car{session.car()};
    const domain::DriftScore& drift{session.drift()};
    const domain::StreetLamps& lamps{session.lamps()};
    const domain::Props& props{session.props()};
    m_pulseLeft = std::max(0.0f, m_pulseLeft - dt);
    // 数が減ったのはステージを変えたとき。揺らさずに合わせる
    m_lastIgnitions = std::min(m_lastIgnitions, lamps.ignitions());
    m_lastHits = std::min(m_lastHits, props.hits());

    if (car.hitWall && !m_wasHitWall && std::fabs(car.speed) > 5.0f)
    {
        pulse(0.9f, 0.7f, 0.25f);
    }
    if (lamps.sinceAllLit() < m_lastSinceAllLit)
    {
        pulse(1.0f, 1.0f, 0.6f);
    }
    if (drift.chaining() && drift.multiplier() > m_lastMultiplier)
    {
        pulse(0.6f, 0.7f, 0.16f);
    }
    if (props.hits() > m_lastHits)
    {
        bool cone{props.lastHitKind() == domain::PropKind::Cone};
        pulse(cone ? 0.25f : 0.55f, cone ? 0.45f : 0.35f, cone ? 0.07f : 0.12f);
    }
    if (lamps.ignitions() > m_lastIgnitions)
    {
        pulse(0.0f, 0.5f, 0.06f);
    }
    m_wasHitWall = car.hitWall;
    m_lastMultiplier = drift.chaining() ? drift.multiplier() : 1;
    m_lastHits = props.hits();
    m_lastIgnitions = lamps.ignitions();
    m_lastSinceAllLit = lamps.sinceAllLit();

    // ドリフト中は、滑り角が深いほど細かく強く震わせる。短い揺れの最中は邪魔しない
    m_sinceSend += dt;
    if (m_pulseLeft > 0.0f || m_sinceSend < SEND_INTERVAL || session.paused())
    {
        return;
    }
    m_sinceSend = 0;
    if (drift.drifting())
    {
        float slide{std::clamp(std::fabs(car.slipAngle()) / 0.7f, 0.0f, 1.0f)};
        m_input.rumble(0.05f + 0.1f * slide, 0.12f + 0.3f * slide, SEND_DURATION_MS);
    }
}

void PadRumble::reset()
{
    m_input.rumble(0.0f, 0.0f, 0);
    m_pulseLeft = 0;
    m_pulseStrength = 0;
    m_sinceSend = 0;
    m_wasHitWall = false;
    m_lastMultiplier = 1;
    // 数はそのまま覚えておく。リセットで数が戻らなくても、次に増えたときだけ揺らす
}

}  // namespace adapters::sdl
