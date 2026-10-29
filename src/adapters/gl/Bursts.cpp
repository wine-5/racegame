#include "adapters/gl/Bursts.hpp"

#include <algorithm>
#include <cmath>

namespace adapters::gl
{

using domain::Vec3;

namespace
{

constexpr std::size_t MAX_PARTICLES = 1200;
constexpr float GRAVITY = 6.0f;  // 少し軽く落として、ふわっと散らす
constexpr float DRAG = 1.6f;     // 空気で遅くなる割合（1/s）

}  // namespace

float Bursts::spread()
{
    m_seed = m_seed * 1664525u + 1013904223u;
    return float(m_seed >> 8) / float(1u << 23) - 1.0f;
}

void Bursts::burst(Vec3 position, Vec3 color, int count, float speed, float size)
{
    for (int i{}; i < count && m_particles.size() < MAX_PARTICLES; ++i)
    {
        Vec3 direction{normalize(Vec3{spread(), spread() * 0.7f + 0.4f, spread()})};
        Particle particle;
        particle.position = position;
        particle.velocity = direction * (speed * (0.45f + 0.55f * std::fabs(spread())));
        particle.color = color;
        particle.life = 0.6f + 0.5f * std::fabs(spread());
        particle.size = size * (0.6f + 0.4f * std::fabs(spread()));
        m_particles.push_back(particle);
    }
}

void Bursts::update(float dt)
{
    float keep{std::exp(-DRAG * dt)};
    for (Particle& particle : m_particles)
    {
        particle.age += dt;
        particle.velocity = particle.velocity * keep;
        particle.velocity.y -= GRAVITY * dt;
        particle.position = particle.position + particle.velocity * dt;
    }
    m_particles.erase(
        std::remove_if(m_particles.begin(), m_particles.end(), [](const Particle& p) { return p.age >= p.life; }),
        m_particles.end());
}

void Bursts::collect(std::vector<Billboard>& out) const
{
    for (const Particle& particle : m_particles)
    {
        float fade{1.0f - particle.age / particle.life};
        Billboard item;
        item.position = particle.position;
        item.size = particle.size * (0.5f + 0.5f * fade);
        item.r = particle.color.x;
        item.g = particle.color.y;
        item.b = particle.color.z;
        item.a = fade * 0.9f;
        out.push_back(item);
    }
}

}  // namespace adapters::gl
