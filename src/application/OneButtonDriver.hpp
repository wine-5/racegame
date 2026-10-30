#pragma once

#include "domain/Car.hpp"
#include "domain/Track.hpp"

namespace application
{

/**
 * @brief ボタン 1 つで遊ぶための運転役
 * @details ステアとアクセルはコースに沿って自動で操作する。プレイヤーはドリフトのボタンだけを押す。
 *          ボタンを押すと、曲がる向きへサイドブレーキで車の後ろを振り出し、押している間はアクセルで滑りを保つ。
 *          ドリフトの角度は、この先のカーブのきつさから決める。離すと角度を戻してグリップ走行に戻る。
 */
class OneButtonDriver
{
public:
    /**
     * @brief 1 ステップ分の入力を作る
     * @param car 車の状態
     * @param track 走っているコース
     * @param driftButton ドリフトのボタンを押しているか
     * @param dt 1 ステップの時間（秒）
     * @param lineOffset 走るライン。中心線から右へずらす距離（m）
     * @return 車に渡す入力
     */
    domain::DriveInput drive(const domain::Car& car, const domain::Track& track, bool driftButton, float dt,
                             float lineOffset = 0.0f);

    /// 前の状態を忘れる。リセットのあとに呼ぶ
    void reset() { *this = OneButtonDriver{}; }

private:
    float m_holdTime{};  // ボタンを押し続けている時間（秒）
    bool m_wasHeld{};
    float m_direction{1.0f};  // ドリフトで曲がる向き。右が 1、左が -1
};

}  // namespace application
