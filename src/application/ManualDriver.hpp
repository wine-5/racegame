#pragma once

#include "application/PlayerControls.hpp"
#include "domain/Car.hpp"

namespace application
{

/**
 * @brief WASD とドリフトのボタンで運転するための運転役
 * @details アクセル・ブレーキ・ステアはそのまま車に渡す。ドリフトのボタンを押すと、
 *          ステアの向きへサイドブレーキで車の後ろを振り出し、押している間は補助が角度を保つ。
 *          ドリフト中のステアは角度の調整になる。曲がる向きへ切ると深く、逆へ切ると浅くなり、
 *          逆へ切り続けると切り返す。
 */
class ManualDriver
{
public:
    /**
     * @brief 1 ステップ分の入力を作る
     * @param car 車の状態
     * @param controls プレイヤーの操作
     * @param dt 1 ステップの時間（秒）
     * @return 車に渡す入力
     */
    domain::DriveInput drive(const domain::Car& car, const PlayerControls& controls, float dt);

    /// 前の状態を忘れる。リセットのあとに呼ぶ
    void reset() { *this = ManualDriver{}; }

private:
    float m_holdTime{};     // ボタンを押し続けている時間（秒）
    float m_counterTime{};  // 曲がる向きと逆へ切り続けている時間（秒）
    bool m_wasHeld{};
    float m_direction{1.0f};  // ドリフトで曲がる向き。右が 1、左が -1
};

}  // namespace application
