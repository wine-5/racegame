#pragma once

#include "adapters/audio/AudioMixer.hpp"
#include "domain/Car.hpp"
#include "domain/DriftScore.hpp"
#include "domain/Props.hpp"
#include "domain/StreetLamps.hpp"

namespace adapters::audio
{

/**
 * @brief 車とドリフトの状態から、鳴らす音を決める
 * @details エンジンは速度から自動でギアを決めて回転数を出し、その高さで鳴らす。シフトアップで回転が落ちる。
 *          スキール音は滑り角が深いほど大きく高くなる。ドリフトの連続の始まり・倍率アップ・得点の確定・
 *          壁への衝突で効果音を鳴らす。街灯が灯ったときと、小物を弾いたときにも鳴らす。
 */
class DriftSound
{
public:
    /**
     * @brief 鳴らす先を指定して作る
     * @param mixer 音を混ぜる先。DriftSound より長く生きていること
     */
    explicit DriftSound(AudioMixer& mixer);

    /**
     * @brief 1 フレーム分、音を更新する
     * @param car 車の状態
     * @param drift ドリフトの得点
     * @param throttle アクセルの踏み具合（0..1）
     * @param dt 前のフレームからの経過時間（秒）
     * @param paused ポーズ中か。ポーズ中はループ音を止める
     */
    void update(const domain::Car& car, const domain::DriftScore& drift, float throttle, float dt, bool paused);

    /**
     * @brief 街灯と小物の変化に合わせて効果音を鳴らす
     * @details 街灯は灯した倍率が高いほど高い音で鳴らす。
     * @param lamps 街灯の状態
     * @param props 小物の状態
     */
    void updateWorld(const domain::StreetLamps& lamps, const domain::Props& props);

    /// 前の状態を忘れる。ステージを変えたときやリセットのあとに呼ぶ
    void reset();

    /// 今のエンジンの回転数（rpm）
    float rpm() const { return m_rpm; }

private:
    /// 速度とアクセルから、目標の回転数を求める
    float targetRpm(const domain::Car& car, float throttle) const;

    AudioMixer& m_mixer;
    float m_rpm{900.0f};
    bool m_wasChaining{};
    int m_lastMultiplier{1};
    float m_lastSinceResult{1e9f};
    bool m_wasHitWall{};
    int m_lastIgnitions{};
    int m_lastHits{};
    float m_lastSinceAllLit{1e9f};
    float m_crashCooldown{};
};

}  // namespace adapters::audio
