#pragma once

#include "domain/Car.hpp"

namespace domain
{

/**
 * @brief ドリフトの得点
 * @details 角度 × 速度 × 時間で点が増える。ドリフトを途切れさせずにつなぐと 1 回の「連続」になり、
 *          左右に切り返すたびに倍率が上がる。連続が終わると点が確定し、壁に当たると連続の点を失う。
 */
class DriftScore
{
public:
    /// 連続の結果
    enum class Result
    {
        None,    // まだ結果が無い
        Banked,  // 点が確定した
        Crashed  // 壁に当たって失った
    };

    /**
     * @brief 1 ステップ分、車の状態から得点を進める
     * @param car 車の状態
     * @param dt 進める時間（秒）
     */
    void update(const Car& car, float dt);

    /// 最初からやり直す
    void reset() { *this = DriftScore{}; }

    /// 今ドリフトしているか
    bool drifting() const { return m_drifting; }

    /// 連続が続いているか（ドリフトが一瞬途切れても、猶予の間は続く）
    bool chaining() const { return m_chaining; }

    /// 今の連続で稼いでいる点（倍率をかける前）
    float chainPoints() const { return m_chainPoints; }

    /// 今の連続の倍率
    int multiplier() const { return m_multiplier; }

    /// 確定した点の合計
    long long total() const { return m_total; }

    /// 最後に終わった連続の結果
    Result lastResult() const { return m_lastResult; }

    /// 最後に終わった連続で確定した点（倍率をかけた後）
    long long lastPoints() const { return m_lastPoints; }

    /// 最後の連続が終わってからの時間（秒）。HUD の表示を消すのに使う
    float sinceLastResult() const { return m_sinceLastResult; }

private:
    /// 連続を終えて、点を確定するか失う
    void finishChain(Result result);

    bool m_drifting{};
    bool m_chaining{};
    float m_chainPoints{};
    int m_multiplier{1};
    int m_direction{};    // 今のドリフトの向き。右へ曲がるとき 1、左へ曲がるとき -1
    float m_graceTime{};  // ドリフトが途切れてからの時間
    long long m_total{};
    Result m_lastResult = Result::None;
    long long m_lastPoints{};
    float m_sinceLastResult{};
};

}  // namespace domain
