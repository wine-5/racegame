#pragma once

namespace application
{

/**
 * @brief プレイヤーの操作（1 フレーム分）
 * @details キーボード・パッドの種類に関係なく、ゲームが受け取る形にまとめたもの。
 */
struct PlayerControls
{
    float throttle{};  // アクセル（0..1）。W
    float brake{};     // ブレーキ（0..1）。S。止まっていれば後退
    float steer{};     // ステア（-1..1）。左が負。A / D
    bool drift{};      // ドリフトのボタンを押しているか。Space
    bool autoSteer{};  // true ならステアとアクセルは自動で、drift だけを使う（タイトルのデモ・画面の保存）
};

}  // namespace application
