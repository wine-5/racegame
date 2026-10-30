#include "application/GameSession.hpp"

namespace application
{

using domain::DriveInput;
using domain::LapTimer;
using domain::Track;
using ports::ITrackSource;

namespace
{

constexpr double MAX_FRAME_SEC = 0.25;
constexpr int MAX_STEPS_PER_FRAME = 8;
constexpr float LAMP_BONUS = 300.0f;  // 街灯を 1 本灯すごとの点
constexpr float PROP_BONUS = 500.0f;  // 小物を 1 つ弾くごとの点

}  // namespace

bool GameSession::start(ITrackSource& source)
{
    m_track = Track{};
    if (!source.load(m_track))
    {
        return false;
    }
    m_props.place(m_track);
    m_lamps.place({}, m_track.length);
    reset();
    return true;
}

void GameSession::reset()
{
    m_car.reset(m_track);
    m_lap = LapTimer{};
    m_drift.reset();
    m_lamps.reset();
    m_props.reset();
    m_autoDriver.reset();
    m_manualDriver.reset();
    m_lastInput = DriveInput{};
}

void GameSession::advance(double elapsedSec, const PlayerControls& controls)
{
    if (elapsedSec > MAX_FRAME_SEC)
        elapsedSec = MAX_FRAME_SEC;
    if (elapsedSec < 0)
        elapsedSec = 0;

    if (m_paused)
    {
        m_accumulator = 0;
        return;
    }
    m_accumulator += elapsedSec;
    int guard{0};
    while (m_accumulator >= FIXED_STEP_SEC && guard < MAX_STEPS_PER_FRAME)
    {
        step(controls);
        m_accumulator -= FIXED_STEP_SEC;
        ++guard;
    }
}

void GameSession::step(const PlayerControls& controls)
{
    const float dt{float(FIXED_STEP_SEC)};
    m_lastInput = controls.autoSteer ? m_autoDriver.drive(m_car, m_track, controls.drift, dt)
                                     : m_manualDriver.drive(m_car, m_track, controls, dt);
    m_car.update(dt, m_lastInput, m_track);
    const int lapsBefore{m_lap.completed()};
    m_lap.tick(m_car.ground.s, m_track.length, dt);
    m_drift.update(m_car, dt);
    if (m_lap.completed() != lapsBefore)
    {
        // 周回が変わったら、街灯を消して小物を戻し、また最初から
        m_lamps.reset();
        m_props.reset();
    }

    const int ignitionsBefore{m_lamps.ignitions()};
    m_lamps.update(m_car, m_drift, dt);
    m_drift.addBonus(LAMP_BONUS * float(m_lamps.ignitions() - ignitionsBefore));
    if (m_lamps.takeAllLitBonus())
    {
        m_drift.addBonus(domain::StreetLamps::ALL_LIT_BONUS);
    }
    const int hitsBefore{m_props.hits()};
    m_props.update(m_car, dt);
    m_drift.addBonus(PROP_BONUS * float(m_props.hits() - hitsBefore));
}

}  // namespace application
