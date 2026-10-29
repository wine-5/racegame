#pragma once

#include "application/ports/ITrackSource.hpp"
#include "domain/Car.hpp"
#include "domain/DriftScore.hpp"
#include "domain/LapTimer.hpp"
#include "domain/Track.hpp"

namespace application
{

/**
 * @brief ゲームを動かす入口（動かす側のポート）
 * @details 外側（入力・メインループ・テスト）から経過時間と入力を受け取り、
 *          固定ステップで車と計測を進める。描画は呼ばない。
 */
class GameSession
{
public:
    static constexpr double FIXED_STEP_SEC = 1.0 / 60.0;

    /**
     * @brief コースを読んで、車をスタート位置に置く
     * @param source コースの読み込み元
     * @return 読み込みに失敗したら false
     */
    bool start(ports::ITrackSource& source);

    /// 車をスタート位置に戻し、計測をやり直す
    void reset();

    /// ポーズを切り替える
    void togglePause() { m_paused = !m_paused; }

    /// ポーズ中かどうか
    bool paused() const { return m_paused; }

    /**
     * @brief 1 フレーム分進める
     * @details たまった時間の分だけ固定ステップで進める。ポーズ中は進めない。
     * @param elapsedSec 前のフレームからの経過時間（秒）
     * @param input 運転の入力
     */
    void advance(double elapsedSec, const domain::DriveInput& input);

    /// 走っているコース
    const domain::Track& track() const { return m_track; }

    /// 車の状態
    const domain::Car& car() const { return m_car; }

    /// 周回の計測
    const domain::LapTimer& lap() const { return m_lap; }

    /// ドリフトの得点
    const domain::DriftScore& drift() const { return m_drift; }

private:
    void step(const domain::DriveInput& input);

    domain::Track m_track;
    domain::Car m_car;
    domain::LapTimer m_lap;
    domain::DriftScore m_drift;
    bool m_paused{};
    double m_accumulator{};
};

}  // namespace application
