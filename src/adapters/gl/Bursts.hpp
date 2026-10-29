#pragma once

#include "adapters/gl/Renderer.hpp"

#include <cstdint>
#include <vector>

namespace adapters::gl
{

/**
 * @brief 一点から光の粒を飛び散らせる演出
 * @details 街灯が灯った瞬間や、小物を弾いた瞬間に使う。粒は重力で落ちながら消えていく。
 */
class Bursts
{
public:
    /**
     * @brief 光の粒を飛び散らせる
     * @param position 飛び散らせる場所
     * @param color 色（0..1）
     * @param count 粒の数
     * @param speed 飛び散る速さ（m/s）
     * @param size 粒の大きさ（m）
     */
    void burst(domain::Vec3 position, domain::Vec3 color, int count, float speed, float size);

    /**
     * @brief 粒を動かす
     * @param dt 経過時間（秒）
     */
    void update(float dt);

    /// 全部消す
    void clear() { m_particles.clear(); }

    /**
     * @brief 描くための板を追加する
     * @param out 追加先
     */
    void collect(std::vector<Billboard>& out) const;

private:
    struct Particle
    {
        domain::Vec3 position;
        domain::Vec3 velocity;
        domain::Vec3 color;
        float age{};
        float life{1.0f};
        float size{0.3f};
    };

    /// -1..1 の乱数
    float spread();

    std::vector<Particle> m_particles;
    std::uint32_t m_seed{4242};
};

}  // namespace adapters::gl
