#pragma once

#include "domain/Car.hpp"
#include "domain/DriftScore.hpp"

#include <vector>

namespace domain
{

/**
 * @brief コース沿いの街灯 1 本
 */
struct StreetLamp
{
    float s{};       // 周回位置（m）
    bool lit{};      // ドリフトで灯したか
    int tier{};      // 灯したときの倍率（1..5）。色に使う
    float litAge{};  // 灯してからの時間（秒）
};

/**
 * @brief ドリフトで灯していく街灯
 * @details ドリフトしながら街灯の横を通ると、そのときの倍率の色で灯る。高い倍率で通り直すと色が上がる。
 *          1 周ですべて灯すとボーナス。周回が変わると消えて、また最初から灯していく。
 */
class StreetLamps
{
public:
    /**
     * @brief 街灯の位置を決める
     * @details 景色を作る側が、実際に街灯を立てた周回位置を渡す。
     * @param spots 周回位置（m）の並び
     * @param trackLength コース 1 周の長さ（m）
     */
    void place(const std::vector<float>& spots, float trackLength);

    /// 全部消す（位置はそのまま）
    void reset();

    /**
     * @brief 1 ステップ分進める
     * @param car 車の状態
     * @param drift ドリフトの得点。ドリフト中かと倍率を見る
     * @param dt 進める時間（秒）
     */
    void update(const Car& car, const DriftScore& drift, float dt);

    /// 街灯の並び
    const std::vector<StreetLamp>& lamps() const { return m_lamps; }

    /// 灯っている数
    int litCount() const { return m_litCount; }

    /// これまでに灯した回数。音や演出が、増えたかどうかで灯った瞬間を知る
    int ignitions() const { return m_ignitions; }

    /// 今の周で全部灯したか
    bool allLit() const { return !m_lamps.empty() && m_litCount == static_cast<int>(m_lamps.size()); }

    /// 全部灯してからの時間（秒）。まだなら大きな値
    float sinceAllLit() const { return m_sinceAllLit; }

    /// 全部灯したときのボーナスを、まだ得点に渡していなければ取り出す
    bool takeAllLitBonus();

private:
    std::vector<StreetLamp> m_lamps;
    float m_trackLength{};
    int m_litCount{};
    int m_ignitions{};
    float m_sinceAllLit{1e9f};
    bool m_bonusPending{};
};

}  // namespace domain
