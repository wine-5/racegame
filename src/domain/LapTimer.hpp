#pragma once

namespace domain
{

/**
 * @brief 周回の計測
 * @details スタートラインを越えたら次の周に入る。最初の周はアウトラップで計らない。
 */
struct LapTimer
{
    int completed{};
    bool timing{};
    float lap{};
    float best{-1};
    float prevS{-1};

    /**
     * @brief 1 ステップ分の時間を進め、ラインを越えたかを調べる
     * @param s コース上の周回位置
     * @param length コースの全長
     * @param dt 進める時間（秒）
     */
    void tick(float s, float length, float dt);
};

}  // namespace domain
