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

}  // namespace

bool GameSession::start(ITrackSource& source)
{
    m_track = Track{};
    if (!source.load(m_track))
    {
        return false;
    }
    reset();
    return true;
}

void GameSession::reset()
{
    m_car.reset(m_track);
    m_lap = LapTimer{};
    m_drift.reset();
}

void GameSession::advance(double elapsedSec, const DriveInput& input)
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
        step(input);
        m_accumulator -= FIXED_STEP_SEC;
        ++guard;
    }
}

void GameSession::step(const DriveInput& input)
{
    m_car.update(float(FIXED_STEP_SEC), input, m_track);
    m_lap.tick(m_car.ground.s, m_track.length, float(FIXED_STEP_SEC));
    m_drift.update(m_car, float(FIXED_STEP_SEC));
}

}  // namespace application
