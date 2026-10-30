#pragma once

#include "adapters/gl/Renderer.hpp"
#include "domain/Car.hpp"
#include "domain/DriftScore.hpp"

#include <cstdint>
#include <vector>

namespace adapters::gl
{

/**
 * @brief 光る火花
 * @details 壁を擦るとオレンジの火花を散らす。ドリフトの倍率が 2 以上になると、後輪から倍率の色の
 *          火花を流す（X2 青、X3 マゼンタ、X4 金、X5 白）。倍率が上がっていくのが一目でわかる。
 */
class Sparks
{
public:
    /**
     * @brief 火花を出し、動かす
     * @param car 車の状態
     * @param drift ドリフトの得点
     * @param dt 前のフレームからの経過時間（秒）
     * @param emit 新しく出すか。ポーズ中は false にする
     */
    void update(const domain::Car& car, const domain::DriftScore& drift, float dt, bool emit);

    /// 全部消す
    void clear() { m_sparks.clear(); }

    /**
     * @brief 描くための板を追加する
     * @param out 追加先
     */
    void collect(std::vector<Billboard>& out) const;

private:
    struct Spark
    {
        domain::Vec3 position;
        domain::Vec3 velocity;
        domain::Vec3 color;
        float age{};
        float life{0.5f};
        float size{0.2f};
        bool heavy{};  // 重力で落ちるか（壁の火花）
    };

    /// -1..1 の乱数
    float spread();

    std::vector<Spark> m_sparks;
    float m_carry{};
    std::uint32_t m_seed{777};
};

}  // namespace adapters::gl
