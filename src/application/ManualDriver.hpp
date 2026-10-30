#pragma once

#include "application/OneButtonDriver.hpp"
#include "application/PlayerControls.hpp"
#include "domain/Car.hpp"
#include "domain/Track.hpp"

namespace application
{

/**
 * @brief WASD とドリフトのボタンで運転するための運転役
 * @details ボタンを押していない間は、アクセル・ブレーキ・ステアをそのまま車に渡す（キーの急なステアは
 *          なめらかにし、後ろが滑り出したらアクセルを戻す）。
 *          ボタンを押している間は OneButtonDriver に任せて、コースに沿ってドリフトを続ける。
 *          ドリフト中の A / D は走るラインを左右へずらす。速度と S 字の切り返しは自動。
 */
class ManualDriver
{
public:
    /**
     * @brief 1 ステップ分の入力を作る
     * @param car 車の状態
     * @param track 走っているコース
     * @param controls プレイヤーの操作
     * @param dt 1 ステップの時間（秒）
     * @return 車に渡す入力
     */
    domain::DriveInput drive(const domain::Car& car, const domain::Track& track, const PlayerControls& controls,
                             float dt);

    /// 前の状態を忘れる。リセットのあとに呼ぶ
    void reset() { *this = ManualDriver{}; }

private:
    OneButtonDriver m_assist;  // ドリフト中の運転
    bool m_wasDrifting{};
    float m_line{};   // ドリフト中に走るライン。中心線から右へのずれ（m）
    float m_steer{};  // なめらかにしたステア（-1..1）
};

}  // namespace application
