#include "adapters/gl/ChaseCamera.hpp"

#include <cmath>

namespace adapters::gl
{

using domain::TrackPose;
using domain::Vec3;

void ChaseCamera::update(const domain::Car& car, const domain::Track& track, float dt)
{
    const float back[MODE_COUNT]{8.6f, 4.1f, 16.0f};
    const float up[MODE_COUNT]{3.3f, 1.55f, 7.2f};
    const float ahead[MODE_COUNT]{9.0f, 6.0f, 14.0f};
    Vec3 forward{std::sin(car.yaw), 0, std::cos(car.yaw)};
    Vec3 position{car.x, car.y, car.z};
    Vec3 eye{position - forward * back[m_mode] + Vec3{0, up[m_mode], 0}};
    Vec3 look{position + forward * ahead[m_mode] + Vec3{0, 1.15f, 0}};
    // 路面より下に潜らない
    TrackPose under{track.query(eye.x, eye.z, car.y, car.ground.s)};
    if (under.found && under.xzDist < 30.0f && eye.y < under.centerY + 0.8f)
    {
        eye.y = under.centerY + 0.8f;
    }
    if (!m_ready)
    {
        m_eye = eye;
        m_look = look;
        m_ready = true;
        return;
    }
    float blend{1.0f - std::exp(-dt * 7.0f)};
    m_eye = m_eye + (eye - m_eye) * blend;
    m_look = m_look + (look - m_look) * blend;
}

const char* ChaseCamera::modeName() const
{
    const char* names[MODE_COUNT]{"CHASE", "CLOSE", "FAR"};
    return names[m_mode];
}

}  // namespace adapters::gl
