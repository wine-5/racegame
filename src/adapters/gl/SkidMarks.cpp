#include "adapters/gl/SkidMarks.hpp"

#include <cmath>

namespace adapters::gl
{

using domain::length;
using domain::Vec3;

namespace
{

constexpr std::size_t MAX_SEGMENTS = 2400;
constexpr float START_SLIP = 0.14f;  // これより滑っていると跡が付く（ラジアン、約 8°）
constexpr float FULL_SLIP = 0.6f;    // この角度で最も濃くなる
constexpr float MIN_SPEED = 4.0f;    // これより遅いと付かない（m/s）
constexpr float STEP = 0.5f;         // この距離ごとに帯を 1 つ足す（m）
constexpr float HALF_WIDTH = 0.13f;  // 帯の幅の半分（m）
constexpr float LIFT = 0.03f;        // 路面から浮かせる高さ（m）
constexpr float WHEEL_X = 0.82f;
constexpr float WHEEL_Z = -1.38f;
// 路面の色（TrackMesh のアスファルト）と、いちばん濃い跡の色
const Vec3 ROAD_COLOR{0.17f, 0.17f, 0.19f};
const Vec3 MARK_COLOR{0.035f, 0.035f, 0.04f};

}  // namespace

bool SkidMarks::update(const domain::Car& car, bool emit)
{
    float slip{std::fabs(car.slipAngle())};
    bool marking{emit && slip > START_SLIP && car.speed > MIN_SPEED};
    if (!marking)
    {
        // 跡は途切れるだけで形は変わらない
        m_wheels[0].active = false;
        m_wheels[1].active = false;
        return false;
    }

    float darkness{std::fmin((slip - START_SLIP) / (FULL_SLIP - START_SLIP), 1.0f)};
    darkness = 0.35f + 0.65f * darkness;
    Vec3 forward{std::sin(car.yaw), 0, std::cos(car.yaw)};
    Vec3 right{std::cos(car.yaw), 0, -std::sin(car.yaw)};
    Vec3 center{car.x, car.y + LIFT, car.z};

    bool changed{};
    for (int i{}; i < 2; ++i)
    {
        float side{i == 0 ? -1.0f : 1.0f};
        Vec3 point{center + right * (side * WHEEL_X) + forward * WHEEL_Z};
        Wheel& wheel{m_wheels[i]};
        if (!wheel.active)
        {
            wheel.active = true;
            wheel.last = point;
            wheel.lastLeft = point;
            wheel.lastRight = point;
            continue;
        }
        Vec3 move{point - wheel.last};
        move.y = 0;
        float distance{length(move)};
        if (distance < STEP)
        {
            continue;
        }
        // 帯の幅は進む向きに直角にとる。ドリフト中は車体の向きと進む向きがずれるため
        Vec3 dir{move * (1.0f / distance)};
        Vec3 across{Vec3{dir.z, 0, -dir.x} * HALF_WIDTH};
        Segment segment;
        segment.a0 = wheel.lastLeft;
        segment.a1 = wheel.lastRight;
        segment.b0 = point - across;
        segment.b1 = point + across;
        if (length(wheel.lastLeft - wheel.lastRight) < 1e-4f)
        {
            // 跡の書き始めは、始まりの端も同じ幅にする
            segment.a0 = wheel.last - across;
            segment.a1 = wheel.last + across;
        }
        segment.darkness = darkness;
        m_segments.push_back(segment);
        wheel.last = point;
        wheel.lastLeft = segment.b0;
        wheel.lastRight = segment.b1;
        changed = true;
    }
    while (m_segments.size() > MAX_SEGMENTS)
    {
        m_segments.pop_front();
    }
    return changed;
}

void SkidMarks::clear()
{
    m_segments.clear();
    m_wheels[0].active = false;
    m_wheels[1].active = false;
}

void SkidMarks::build(CpuMesh& mesh) const
{
    mesh = CpuMesh{};
    const Vec3 up{0, 1, 0};
    const std::size_t count{m_segments.size()};
    for (std::size_t i{}; i < count; ++i)
    {
        const Segment& segment{m_segments[i]};
        // 古い跡ほど路面の色に近づける。いちばん新しい跡が 0、消える直前が 1
        float age{float(count - 1 - i) / float(MAX_SEGMENTS)};
        float amount{segment.darkness * (1.0f - 0.65f * age)};
        Vec3 color{ROAD_COLOR + (MARK_COLOR - ROAD_COLOR) * amount};
        mesh.addQuad(segment.a0, segment.a1, segment.b1, segment.b0, up, color);
    }
}

}  // namespace adapters::gl
