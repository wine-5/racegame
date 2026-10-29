#pragma once

#include "domain/Track.hpp"

namespace domain
{

struct DriveInput
{
    float throttle{};  // 0..1
    float brake{};     // 0..1  停止中は後退
    float steer{};     // -1..1  左が負、右が正
};

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

    void reset(const Track& track);
    void update(float dt, const DriveInput& input, const Track& track);

    // グリップしたまま進む。ドリフトの課題ではこの関数を差し替える。
    void stepGrip(float dt);
};

}  // namespace domain
