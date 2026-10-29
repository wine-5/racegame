#pragma once

#include "application/ports/TrackSource.hpp"
#include "domain/Car.hpp"
#include "domain/LapTimer.hpp"
#include "domain/Track.hpp"

namespace application
{

// ポート: 外側（入力・メインループ・テスト）がゲームを動かすための入口。
// 経過時間と入力を受け取り、固定ステップで車と計測を進める。描画は呼ばない。
class GameSession
{
public:
    static constexpr double kFixedStepSec = 1.0 / 60.0;

    // コースを読んで、車をスタート位置に置く。
    bool start(ports::TrackSource& source);
    void reset();
    void togglePause() { paused_ = !paused_; }
    bool paused() const { return paused_; }

    // 1 フレームの経過時間を渡す。たまった時間の分だけ固定ステップで進める。
    void advance(double elapsedSec, const domain::DriveInput& input);

    const domain::Track& track() const { return track_; }
    const domain::Car& car() const { return car_; }
    const domain::LapTimer& lap() const { return lap_; }

private:
    void step(const domain::DriveInput& input);

    domain::Track track_;
    domain::Car car_;
    domain::LapTimer lap_;
    bool paused_ = false;
    double accumulator_ = 0;
};

}  // namespace application
