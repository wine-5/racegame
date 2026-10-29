#pragma once

#include "adapters/gl/Renderer.hpp"
#include "domain/Car.hpp"

#include <cstdint>
#include <vector>

namespace adapters::gl
{

/**
 * @brief 後輪から出るタイヤスモーク
 * @details 滑っているほど、速いほど多く出る。煙は少し浮き上がりながら広がって消える。
 *          見た目だけなので描画側に置き、ゲームの状態は書き換えない。
 */
class TireSmoke
{
public:
    /**
     * @brief 煙を出し、動かす
     * @param car 車の状態
     * @param dt 前のフレームからの経過時間（秒）
     * @param emit 新しく煙を出すか。ポーズ中は false にする
     */
    void update(const domain::Car& car, float dt, bool emit);

    /// 全部消す。リセットのあとに呼ぶ
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
        float age{};
        float life{1.0f};
        float startSize{0.5f};
        float endSize{3.0f};
        float opacity{0.5f};
        float groundY{};  // 出たときの路面の高さ。板が路面に刺さらないようにする
    };

    /// 0..1 の乱数。毎回同じ並びになるよう自前で持つ
    float random();

    /// 1 か所から煙を 1 つ出す
    void spawn(domain::Vec3 position, domain::Vec3 carVelocity, float intensity, float groundY);

    std::vector<Particle> m_particles;
    float m_emitCarry{};  // 端数の煙を次のフレームへ持ち越す
    std::uint32_t m_seed{12345};
};

}  // namespace adapters::gl
