#pragma once

#include "domain/Track.hpp"

struct DriveInput {
    float throttle = 0;  // 0..1
    float brake = 0;     // 0..1  停止中は後退
    float steer = 0;     // -1..1  左が負、右が正
};

struct Car {
    float x = 0;
    float y = 0;
    float z = 0;
    float yaw = 0;
    float speed = 0;        // m/s。正が前進
    float steerAngle = 0;   // 前輪の切れ角（ラジアン、右が正）
    float wheelSpin = 0;
    float brakeHold = 0;    // 止まってから後退に入るまでの待ち時間
    bool onAsphalt = true;
    TrackPose ground;

    void reset(const Track& track);
    void update(float dt, const DriveInput& input, const Track& track);

    // グリップしたまま進む。ドリフトの課題ではこの関数を差し替える。
    void stepGrip(float dt);
};
