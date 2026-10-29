#pragma once

namespace domain
{

// 周回の計測。スタートラインを越えたら次の周に入る。最初の周はアウトラップで計らない。
struct LapTimer
{
    int completed{};
    bool timing{};
    float lap{};
    float best{-1};
    float prevS{-1};

    // s はコース上の周回位置、length はコースの全長。
    void tick(float s, float length, float dt);
};

}  // namespace domain
