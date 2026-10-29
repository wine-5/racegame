#pragma once

#include "domain/Track.hpp"

namespace domain
{

/**
 * @brief 運転の入力。キーボード・パッド・自動運転のどれからでも作れる
 */
struct DriveInput
{
    float throttle{};  // 0..1
    float brake{};     // 0..1  停止中は後退
    float steer{};     // -1..1  左が負、右が正
};

/**
 * @brief 車の状態と物理
 */
struct Car
{
    float x{};
    float y{};
    float z{};
    float yaw{};
    float speed{};       // m/s。正が前進
    float steerAngle{};  // 前輪の切れ角（ラジアン、右が正）
    float wheelSpin{};
    float brakeHold{};  // 止まってから後退に入るまでの待ち時間
    bool onAsphalt{true};
    TrackPose ground;

    /**
     * @brief スタート位置に戻して止める
     * @param track 走るコース
     */
    void reset(const Track& track);

    /**
     * @brief 1 ステップ進める
     * @details ステアリング、アクセル・ブレーキ、移動、壁との接触、路面の高さを順に処理する。
     * @param dt 進める時間（秒）
     * @param input 運転の入力
     * @param track 走るコース
     */
    void update(float dt, const DriveInput& input, const Track& track);

    /**
     * @brief グリップしたまま進む
     * @details 横滑りはしない。ドリフトの課題ではこの関数を差し替える。
     * @param dt 進める時間（秒）
     */
    void stepGrip(float dt);
};

}  // namespace domain
