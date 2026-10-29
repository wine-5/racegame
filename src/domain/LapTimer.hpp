#pragma once

namespace domain
{

/**
 * @brief 周回の計測
 * @details スタートラインを越えたら次の周に入る。最初の周はアウトラップで計らない。
 */
class LapTimer
{
public:
    /**
     * @brief 1 ステップ分の時間を進め、ラインを越えたかを調べる
     * @param s コース上の周回位置
     * @param length コースの全長
     * @param dt 進める時間（秒）
     */
    void tick(float s, float length, float dt);

    /// 計測を終えた周の数
    int completed() const { return m_completed; }

    /// 計測中か。最初にラインを越えるまでのアウトラップでは false
    bool timing() const { return m_timing; }

    /// 今の周の経過時間（秒）
    float lapTime() const { return m_lapTime; }

    /// いちばん速い周のタイム（秒）。まだ無ければ負
    float bestTime() const { return m_bestTime; }

private:
    int m_completed{};
    bool m_timing{};
    float m_lapTime{};
    float m_bestTime{-1};
    float m_prevS{-1};
};

}  // namespace domain
