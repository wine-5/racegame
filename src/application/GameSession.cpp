#include "application/GameSession.hpp"

namespace application
{

using domain::DriveInput;
using domain::LapTimer;
using domain::Track;
using ports::ITrackSource;

namespace
{

constexpr double kMaxFrameSec = 0.25;
constexpr int kMaxStepsPerFrame = 8;

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
}

void GameSession::advance(double elapsedSec, const DriveInput& input)
{
    if (elapsedSec > kMaxFrameSec)
        elapsedSec = kMaxFrameSec;
    if (elapsedSec < 0)
        elapsedSec = 0;

    if (m_paused)
    {
        m_accumulator = 0;
        return;
    }
    m_accumulator += elapsedSec;
    int guard = 0;
    while (m_accumulator >= kFixedStepSec && guard < kMaxStepsPerFrame)
    {
        step(input);
        m_accumulator -= kFixedStepSec;
        ++guard;
    }
}

void GameSession::step(const DriveInput& input)
{
    m_car.update(float(kFixedStepSec), input, m_track);
    m_lap.tick(m_car.ground.s, m_track.length, float(kFixedStepSec));
}

}  // namespace application
