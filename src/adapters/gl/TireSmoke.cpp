#include "adapters/gl/TireSmoke.hpp"

#include <algorithm>
#include <cmath>

namespace adapters::gl
{

using domain::length;
using domain::Vec3;

namespace
{

constexpr std::size_t MAX_PARTICLES = 700;
constexpr float START_SLIP = 0.12f;         // これより滑っていると煙が出る（ラジアン、約 7°）
constexpr float FULL_SLIP = 0.6f;           // この角度で最も濃くなる（約 34°）
constexpr float MIN_SPEED = 4.0f;           // これより遅いと出ない（m/s）
constexpr float EMIT_RATE = 45.0f;          // 最も濃いときに 1 秒あたりタイヤ 1 本から出る数
constexpr float WHEEL_X = 0.82f;            // 後輪の左右の位置（m）
constexpr float WHEEL_Z = -1.38f;           // 後輪の前後の位置（m）
constexpr float CAR_VELOCITY_KEEP = 0.25f;  // 煙が車の速度を受け継ぐ割合
constexpr float DRAG = 1.6f;                // 煙が空気で止まる速さ
constexpr float RISE = 0.9f;                // 煙が浮き上がる速さ（m/s²）
constexpr float FULL_SPEED = 16.0f;         // この速度で煙の量が最大になる（m/s、約 58 km/h）
constexpr float NEAR_FADE = 6.0f;           // カメラからこの距離より近い煙は薄くする（m）

}  // namespace

float TireSmoke::random()
{
    m_seed = m_seed * 1664525u + 1013904223u;
    return float(m_seed >> 8) / float(1u << 24);
}

void TireSmoke::spawn(Vec3 position, Vec3 carVelocity, float intensity, float groundY)
{
    if (m_particles.size() >= MAX_PARTICLES)
    {
        return;
    }
    Particle particle;
    particle.position = position + Vec3{random() - 0.5f, random() * 0.2f, random() - 0.5f} * 0.4f;
    particle.velocity =
        carVelocity * CAR_VELOCITY_KEEP + Vec3{random() - 0.5f, random() * 0.6f, random() - 0.5f} * 2.0f;
    particle.life = 1.2f + random() * 0.9f;
    particle.startSize = 0.5f + random() * 0.4f;
    particle.endSize = 3.0f + random() * 2.0f + intensity * 1.5f;
    particle.opacity = 0.12f + 0.23f * intensity;
    particle.groundY = groundY;
    m_particles.push_back(particle);
}

void TireSmoke::update(const domain::Car& car, float dt, bool emit)
{
    for (Particle& particle : m_particles)
    {
        particle.age += dt;
        particle.velocity = particle.velocity * std::exp(-DRAG * dt) + Vec3{0, RISE * dt, 0};
        particle.position = particle.position + particle.velocity * dt;
    }
    m_particles.erase(std::remove_if(m_particles.begin(), m_particles.end(),
                                     [](const Particle& particle) { return particle.age >= particle.life; }),
                      m_particles.end());

    float slip{std::fabs(car.slipAngle())};
    if (!emit || slip < START_SLIP || car.speed < MIN_SPEED)
    {
        m_emitCarry = 0;
        return;
    }
    float intensity{std::fmin((slip - START_SLIP) / (FULL_SLIP - START_SLIP), 1.0f)};
    // 遅いと煙がその場にたまって視界をふさぐので、量を減らす
    intensity *= std::fmin(car.speed / FULL_SPEED, 1.0f);
    Vec3 forward{std::sin(car.yaw), 0, std::cos(car.yaw)};
    Vec3 right{std::cos(car.yaw), 0, -std::sin(car.yaw)};
    Vec3 center{car.x, car.y + 0.25f, car.z};
    Vec3 carVelocity{forward * car.speed + right * car.lateralSpeed};

    m_emitCarry += EMIT_RATE * (0.3f + 0.7f * intensity) * dt;
    while (m_emitCarry >= 1.0f)
    {
        m_emitCarry -= 1.0f;
        for (int side{-1}; side <= 1; side += 2)
        {
            Vec3 wheel{center + right * (float(side) * WHEEL_X) + forward * WHEEL_Z};
            spawn(wheel, carVelocity, intensity, car.y);
        }
    }
}

void TireSmoke::collect(std::vector<Billboard>& out, Vec3 cameraPos) const
{
    for (const Particle& particle : m_particles)
    {
        float t{particle.age / particle.life};
        // 出た直後は少しずつ濃くなり、そのあとゆっくり薄れる
        float fade{std::fmin(t * 8.0f, 1.0f) * (1.0f - t) * (1.0f - t)};
        Billboard item;
        item.size = particle.startSize + (particle.endSize - particle.startSize) * std::sqrt(t);
        item.position = particle.position;
        // 板の下の端が路面より下にくると、路面との境目がくっきり見えてしまう
        item.position.y = std::fmax(item.position.y, particle.groundY + item.size * 0.45f);
        item.r = 0.86f;
        item.g = 0.86f;
        item.b = 0.88f;
        float near{std::fmin(length(item.position - cameraPos) / NEAR_FADE, 1.0f)};
        item.a = particle.opacity * fade * near * near;
        out.push_back(item);
    }
}

}  // namespace adapters::gl
