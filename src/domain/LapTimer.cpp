#include "domain/LapTimer.hpp"

namespace domain
{

void LapTimer::tick(float s, float length, float dt)
{
    if (m_prevS >= 0)
    {
        float gap{(length - m_prevS) + s};
        if (m_prevS > length * 0.5f && s < length * 0.5f && gap < 160.0f)
        {
            if (m_timing)
            {
                if (m_bestTime < 0 || m_lapTime < m_bestTime)
                {
                    m_bestTime = m_lapTime;
                }
                m_completed += 1;
            }
            m_timing = true;
            m_lapTime = 0;
        }
    }
    m_prevS = s;
    m_lapTime += dt;
}

}  // namespace domain
