#include "adapters/gl/Sparks.hpp"

#include <algorithm>
#include <cmath>

namespace adapters::gl
{

using domain::Vec3;

namespace
{

constexpr std::size_t MAX_SPARKS = 900;
constexpr int WALL_BURST = 7;        // 壁に当たっている間、1 フレームに出す数
constexpr float DRIFT_RATE = 70.0f;  // 倍率 X5 のとき、1 秒あたりタイヤ 1 本から出す数
constexpr float GRAVITY = 9.8f;
constexpr float WHEEL_X = 0.82f;
constexpr float WHEEL_Z = -1.38f;

Vec3 multiplierColor(int multiplier)
{
    switch (multiplier)
    {
        case 2:
            return {0.25f, 0.85f, 1.0f};
        case 3:
            return {1.0f, 0.25f, 0.85f};
        case 4:
            return {1.0f, 0.8f, 0.2f};
        default:
            return {1.0f, 1.0f, 1.0f};
    }
}

}  // namespace

float Sparks::spread()
{
    m_seed = m_seed * 1664525u + 1013904223u;
    return float(m_seed >> 8) / float(1u << 23) - 1.0f;
}

void Sparks::update(const domain::Car& car, const domain::DriftScore& drift, float dt, bool emit)
{
    for (Spark& spark : m_sparks)
    {
        spark.age += dt;
        if (spark.heavy)
        {
            spark.velocity.y -= GRAVITY * dt;
        }
        spark.velocity = spark.velocity * std::exp(-1.5f * dt);
        spark.position = spark.position + spark.velocity * dt;
        // 路面で跳ねる
        if (spark.position.y < car.y + 0.02f && spark.velocity.y < 0)
        {
            spark.velocity.y *= -0.35f;
            spark.position.y = car.y + 0.02f;
        }
    }
    m_sparks.erase(
        std::remove_if(m_sparks.begin(), m_sparks.end(), [](const Spark& spark) { return spark.age >= spark.life; }),
        m_sparks.end());
    if (!emit)
    {
        return;
    }

    Vec3 forward{std::sin(car.yaw), 0, std::cos(car.yaw)};
    Vec3 right{std::cos(car.yaw), 0, -std::sin(car.yaw)};
    Vec3 center{car.x, car.y, car.z};
    Vec3 velocity{forward * car.speed + right * car.lateralSpeed};

    // 壁を擦ったところから、進む向きへ火花を散らす
    if (car.hitWall && std::fabs(car.speed) > 4.0f)
    {
        float side{car.ground.lateral > 0 ? 1.0f : -1.0f};
        Vec3 contact{center + right * (side * 1.0f) + Vec3{0, 0.4f, 0}};
        for (int i{}; i < WALL_BURST && m_sparks.size() < MAX_SPARKS; ++i)
        {
            Spark spark;
            spark.position = contact + forward * (spread() * 1.5f);
            spark.velocity = velocity * 0.6f + Vec3{spread() * 3.0f, 2.0f + spread() * 2.0f, spread() * 3.0f};
            spark.color = Vec3{1.0f, 0.55f + 0.25f * spread(), 0.12f};
            spark.life = 0.35f + 0.3f * (spread() * 0.5f + 0.5f);
            spark.size = 0.16f + 0.06f * spread();
            spark.heavy = true;
            m_sparks.push_back(spark);
        }
    }

    // 倍率が 2 以上のドリフト中は、後輪から倍率の色の火花を流す
    if (!drift.drifting() || drift.multiplier() < 2)
    {
        m_carry = 0;
        return;
    }
    m_carry += DRIFT_RATE * float(drift.multiplier() - 1) / 4.0f * dt;
    Vec3 color{multiplierColor(drift.multiplier())};
    while (m_carry >= 1.0f && m_sparks.size() < MAX_SPARKS)
    {
        m_carry -= 1.0f;
        for (float side{-1.0f}; side <= 1.0f; side += 2.0f)
        {
            Spark spark;
            spark.position = center + right * (side * WHEEL_X) + forward * WHEEL_Z + Vec3{0, 0.15f, 0};
            spark.velocity =
                velocity * 0.35f - forward * 3.0f + Vec3{spread() * 1.5f, 1.0f + spread() * 0.8f, spread() * 1.5f};
            spark.color = color;
            spark.life = 0.35f + 0.25f * (spread() * 0.5f + 0.5f);
            spark.size = 0.22f + 0.06f * spread();
            spark.heavy = false;
            m_sparks.push_back(spark);
        }
    }
}

void Sparks::collect(std::vector<Billboard>& out) const
{
    for (const Spark& spark : m_sparks)
    {
        float t{spark.age / spark.life};
        Billboard item;
        item.position = spark.position;
        item.size = spark.size * (1.0f - 0.5f * t);
        item.r = spark.color.x;
        item.g = spark.color.y;
        item.b = spark.color.z;
        item.a = 1.0f - t;
        out.push_back(item);
    }
}

}  // namespace adapters::gl
