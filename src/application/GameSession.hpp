#pragma once

#include "application/ports/ITrackSource.hpp"
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
    bool start(ports::ITrackSource& source);
    void reset();
    void togglePause() { m_paused = !m_paused; }
    bool paused() const { return m_paused; }

    // 1 フレームの経過時間を渡す。たまった時間の分だけ固定ステップで進める。
    void advance(double elapsedSec, const domain::DriveInput& input);

    const domain::Track& track() const { return m_track; }
    const domain::Car& car() const { return m_car; }
    const domain::LapTimer& lap() const { return m_lap; }

private:
    void step(const domain::DriveInput& input);

    domain::Track m_track;
    domain::Car m_car;
    domain::LapTimer m_lap;
    bool m_paused = false;
    double m_accumulator = 0;
};

}  // namespace application
