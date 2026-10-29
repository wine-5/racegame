#include "domain/DriftScore.hpp"

#include <cmath>

namespace domain
{

namespace
{

constexpr float MIN_ANGLE = 0.17f;  // これより浅いとドリフトと数えない（ラジアン、約 10°）
constexpr float MIN_SPEED = 8.0f;   // これより遅いとドリフトと数えない（m/s、約 29 km/h）
constexpr float GRACE_TIME = 1.2f;  // ドリフトが途切れても、この時間内に戻れば連続が続く（秒）
constexpr float POINT_RATE = 0.5f;  // 1 秒あたりの点 = 角度（度）× 速度（m/s）× これ
constexpr int MAX_MULTIPLIER = 5;
constexpr float RAD_TO_DEG = 57.29578f;

}  // namespace

void DriftScore::update(const Car& car, float dt)
{
    m_sinceLastResult += dt;
    m_sinceMultiplierUp += dt;

    if (m_chaining && car.hitWall)
    {
        finishChain(Result::Crashed);
    }

    float slip{car.slipAngle()};
    m_drifting = std::fabs(slip) > MIN_ANGLE && car.speed > MIN_SPEED && !car.hitWall;
    if (m_drifting)
    {
        // 右へ曲がるドリフトでは進む向きが車体の左になるので slip は負
        int direction{slip < 0 ? 1 : -1};
        if (!m_chaining)
        {
            m_chaining = true;
            m_chainPoints = 0;
            m_multiplier = 1;
        }
        else if (direction != m_direction && m_multiplier < MAX_MULTIPLIER)
        {
            // 左右の切り返しで倍率が上がる
            ++m_multiplier;
            m_sinceMultiplierUp = 0;
        }
        m_direction = direction;
        m_graceTime = 0;
        m_chainPoints += std::fabs(slip) * RAD_TO_DEG * car.speed * POINT_RATE * dt;
        return;
    }

    if (m_chaining)
    {
        m_graceTime += dt;
        if (m_graceTime > GRACE_TIME)
        {
            finishChain(Result::Banked);
        }
    }
}

void DriftScore::finishChain(Result result)
{
    m_lastResult = result;
    m_lastPoints = result == Result::Banked ? static_cast<long long>(m_chainPoints) * m_multiplier : 0;
    m_total += m_lastPoints;
    m_sinceLastResult = 0;
    m_chaining = false;
    m_chainPoints = 0;
    m_multiplier = 1;
    m_direction = 0;
    m_graceTime = 0;
}

}  // namespace domain
