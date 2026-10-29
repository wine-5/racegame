#include "adapters/gl/LightTrail.hpp"

#include <algorithm>
#include <cmath>

namespace adapters::gl
{

using domain::length;
using domain::Vec3;

namespace
{

constexpr float LIFE = 0.7f;        // 尾が消えるまでの時間（秒）
constexpr float START_SLIP = 0.2f;  // これより滑っていると尾を引く（ラジアン、約 11°）
constexpr float MIN_SPEED = 6.0f;   // これより遅いと引かない（m/s）
constexpr float SPACING = 0.18f;    // 板を並べる間隔（m）
constexpr std::size_t MAX_POINTS = 240;
constexpr float TAIL_X = 0.67f;  // テールランプの左右の位置（m）
constexpr float TAIL_Y = 0.52f;
constexpr float TAIL_Z = -2.08f;
const Vec3 RED{1.0f, 0.12f, 0.08f};

}  // namespace

void LightTrail::clear()
{
    m_points.clear();
    m_active = false;
}

void LightTrail::update(const domain::Car& car, float dt, bool emit)
{
    for (Point& point : m_points)
    {
        point.age += dt;
    }
    while (!m_points.empty() && (m_points.front().age > LIFE || m_points.size() > MAX_POINTS))
    {
        m_points.pop_front();
    }
    float slip{std::fabs(car.slipAngle())};
    if (!emit || slip < START_SLIP || car.speed < MIN_SPEED)
    {
        m_active = false;
        return;
    }
    Vec3 forward{std::sin(car.yaw), 0, std::cos(car.yaw)};
    Vec3 right{std::cos(car.yaw), 0, -std::sin(car.yaw)};
    Vec3 base{Vec3{car.x, car.y + TAIL_Y, car.z} + forward * TAIL_Z};
    Point point;
    point.left = base - right * TAIL_X;
    point.right = base + right * TAIL_X;
    point.strength = std::fmin((slip - START_SLIP) / 0.5f, 1.0f);
    point.start = !m_active;
    m_points.push_back(point);
    m_active = true;
}

void LightTrail::collect(std::vector<Billboard>& out) const
{
    // 記録した点の間を細かく埋めて、線に見せる
    for (std::size_t i{1}; i < m_points.size(); ++i)
    {
        const Point& a{m_points[i - 1]};
        const Point& b{m_points[i]};
        if (b.start)
        {
            continue;
        }
        float fade{1.0f - b.age / LIFE};
        float alpha{fade * fade * (0.35f + 0.65f * b.strength)};
        float size{0.16f + 0.12f * b.strength};
        for (int side{}; side < 2; ++side)
        {
            Vec3 from{side == 0 ? a.left : a.right};
            Vec3 to{side == 0 ? b.left : b.right};
            float distance{length(to - from)};
            int steps{std::max(1, static_cast<int>(distance / SPACING))};
            for (int k{}; k < steps; ++k)
            {
                Billboard item;
                item.position = from + (to - from) * (float(k) / float(steps));
                item.size = size;
                item.r = RED.x;
                item.g = RED.y;
                item.b = RED.z;
                item.a = alpha;
                out.push_back(item);
            }
        }
    }
}

}  // namespace adapters::gl
