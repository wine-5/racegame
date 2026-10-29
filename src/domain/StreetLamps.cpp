#include "domain/StreetLamps.hpp"

#include <cmath>

namespace domain
{

namespace
{

constexpr float LIGHT_RANGE = 9.0f;  // 周回位置でこの距離（m）以内を通ると灯る

}  // namespace

void StreetLamps::place(const std::vector<float>& spots, float trackLength)
{
    m_lamps.clear();
    for (float s : spots)
    {
        m_lamps.push_back(StreetLamp{s});
    }
    m_trackLength = trackLength;
    m_ignitions = 0;
    reset();
}

void StreetLamps::reset()
{
    for (StreetLamp& lamp : m_lamps)
    {
        lamp = StreetLamp{lamp.s};
    }
    m_litCount = 0;
    m_sinceAllLit = 1e9f;
    m_bonusPending = false;
}

void StreetLamps::update(const Car& car, const DriftScore& drift, float dt)
{
    m_sinceAllLit += dt;
    for (StreetLamp& lamp : m_lamps)
    {
        lamp.litAge += dt;
    }
    if (!drift.drifting() || !car.ground.found || m_trackLength <= 0.0f)
    {
        return;
    }
    const bool wasAllLit{allLit()};
    for (StreetLamp& lamp : m_lamps)
    {
        float gap{std::fabs(car.ground.s - lamp.s)};
        gap = std::fmin(gap, m_trackLength - gap);
        if (gap > LIGHT_RANGE || (lamp.lit && lamp.tier >= drift.multiplier()))
        {
            continue;
        }
        if (!lamp.lit)
        {
            ++m_litCount;
        }
        lamp.lit = true;
        lamp.tier = drift.multiplier();
        lamp.litAge = 0;
        ++m_ignitions;
    }
    if (!wasAllLit && allLit())
    {
        m_sinceAllLit = 0;
        m_bonusPending = true;
    }
}

bool StreetLamps::takeAllLitBonus()
{
    bool pending{m_bonusPending};
    m_bonusPending = false;
    return pending;
}

}  // namespace domain
