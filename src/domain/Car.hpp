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
    bool handbrake{};  // 後輪だけをロックする。ドリフトのきっかけに使う
    // ドリフト中に保つ角度（ラジアン）。負なら、ステアとアクセルから角度を決める
    float driftAngle{-1.0f};
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
    float speed{};         // m/s。車体の前向きの速度。正が前進
    float lateralSpeed{};  // m/s。車体の横向きの速度。右が正
    float yawRate{};       // rad/s。右回りが正
    float steerAngle{};    // 前輪の切れ角（ラジアン、右が正）
    float wheelSpin{};
    float brakeHold{};  // 止まってから後退に入るまでの待ち時間
    bool onAsphalt{true};
    bool hitWall{};  // 直前のステップで壁に当たったか
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
     * @brief 車体の向きと進む向きのずれ（ドリフトの角度）を取得する
     * @return ラジアン。進む向きが車体より右なら正。ほぼ止まっているときは 0
     */
    float slipAngle() const;

    /**
     * @brief 前輪をどこまで切れるか
     * @return 切れ角の上限（ラジアン）。ドリフト中は深く切れる
     */
    float maxLock() const;

    /**
     * @brief ステア入力 1 のときの切れ角
     * @return ラジアン。速いほど小さく、滑っているほど大きい
     */
    float steerLimit() const;

    /**
     * @brief 前輪をこの角度にしたいときのステア入力を求める
     * @details 自動運転が使う。カウンターステアの補助の分を差し引いて逆算する。
     * @param wheelAngle 車体に対する前輪の角度（ラジアン、右が正）
     * @return ステア入力（-1..1）
     */
    float steerInputFor(float wheelAngle) const;

    /**
     * @brief グリップしたまま進む
     * @details 横滑りはしない。低速で、タイヤの力の計算が不安定になるときに使う。
     * @param dt 進める時間（秒）
     */
    void stepGrip(float dt);

    /**
     * @brief タイヤの滑り角から横向きの力を求めて進む
     * @details 前後のタイヤの横向きの力で、横向きの速度と回転の速さを変える。
     *          アクセルとサイドブレーキで後輪のグリップが減り、ドリフトになる。
     * @param dt 進める時間（秒）
     * @param input 運転の入力
     */
    void stepTire(float dt, const DriveInput& input);
};

}  // namespace domain
