#include "domain/CourseBuilder.hpp"

#include <algorithm>
#include <cmath>

namespace domain
{

namespace
{

constexpr float SAMPLE_STEP = 2.0f;   // 点の間隔（m）
constexpr float WIDTH_BLEND = 24.0f;  // 道幅が変わるのにかける距離（m）
constexpr float SPAWN_BEHIND = 12.0f;  // スタート位置をスタートラインのどれだけ手前にするか（m）
constexpr float PI = 3.14159265f;

}  // namespace

CourseBuilder::CourseBuilder(float x, float z, float yaw, float halfWidth)
    : m_x(x), m_z(z), m_yaw(yaw), m_halfWidth(halfWidth), m_startX(x), m_startZ(z)
{
}

void CourseBuilder::section(const std::string& name)
{
    m_names.push_back(name);
    m_nameId = static_cast<int>(m_names.size()) - 1;
}

float CourseBuilder::blendWidth(float target, float travelled) const
{
    float t{std::fmin(travelled / WIDTH_BLEND, 1.0f)};
    // 端がなめらかになるよう smoothstep で変える
    t = t * t * (3.0f - 2.0f * t);
    return m_halfWidth + (target - m_halfWidth) * t;
}

void CourseBuilder::addSample(float halfWidth)
{
    TrackSample sample;
    sample.x = m_x;
    sample.z = m_z;
    sample.halfWidth = halfWidth;
    sample.nameId = m_nameId;
    m_samples.push_back(sample);
}

void CourseBuilder::straight(float length, float halfWidth)
{
    int steps{std::max(1, static_cast<int>(std::lround(length / SAMPLE_STEP)))};
    float step{length / float(steps)};
    for (int i{}; i < steps; ++i)
    {
        addSample(blendWidth(halfWidth, float(i) * step));
        m_x += std::sin(m_yaw) * step;
        m_z += std::cos(m_yaw) * step;
    }
    m_halfWidth = halfWidth;
}

void CourseBuilder::arc(float radius, float degrees, float halfWidth)
{
    float angle{degrees * PI / 180.0f};
    float length{std::fabs(angle) * radius};
    int steps{std::max(1, static_cast<int>(std::lround(length / SAMPLE_STEP)))};
    float turn{angle / float(steps)};
    // 弧を細かい弦でたどる。弦の向きは弧の中ほどの向きにすると、ずれがたまらない
    float chord{2.0f * radius * std::sin(std::fabs(turn) * 0.5f)};
    for (int i{}; i < steps; ++i)
    {
        addSample(blendWidth(halfWidth, float(i) * (length / float(steps))));
        float mid{m_yaw + turn * 0.5f};
        m_x += std::sin(mid) * chord;
        m_z += std::cos(mid) * chord;
        m_yaw += turn;
    }
    m_halfWidth = halfWidth;
}

float CourseBuilder::closureGap() const
{
    return std::hypot(m_x - m_startX, m_z - m_startZ);
}

bool CourseBuilder::build(Track& track) const
{
    track.samples = m_samples;
    track.names = m_names;
    if (!track.finalize())
    {
        return false;
    }
    // スタートラインの手前、最後の区間の上に置く
    int back{static_cast<int>(SPAWN_BEHIND / SAMPLE_STEP)};
    int n{static_cast<int>(track.samples.size())};
    const TrackSample& spawn{track.samples[(n - back) % n]};
    const TrackSample& next{track.samples[(n - back + 1) % n]};
    track.spawnX = spawn.x;
    track.spawnY = spawn.y;
    track.spawnZ = spawn.z;
    track.spawnYaw = std::atan2(next.x - spawn.x, next.z - spawn.z);
    return true;
}

}  // namespace domain
