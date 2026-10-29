#pragma once

#include "domain/Car.hpp"
#include "domain/Geom.hpp"
#include "domain/Track.hpp"

#include <cmath>

namespace adapters::gl
{

/**
 * @brief 車の後ろから追いかけるカメラ
 * @details 見せ方の問題なので描画側に置く。路面より下に潜らないようにだけコースを参照する。
 */
class ChaseCamera
{
public:
    /**
     * @brief 車を追いかけて視点を動かす
     * @param car 追いかける車
     * @param track 走っているコース
     * @param dt 前のフレームからの経過時間（秒）
     */
    void update(const domain::Car& car, const domain::Track& track, float dt);

    /// 次のフレームで、補間せずに目標の位置へ飛ぶ
    void snap() { m_ready = false; }

    /// CHASE → CLOSE → FAR の順に切り替える
    void cycleMode() { m_mode = (m_mode + 1) % MODE_COUNT; }

    /**
     * @brief カメラを揺らす
     * @details 揺れはすぐに収まる。強い揺れを受けている間に弱い揺れが来ても、強い方が残る。
     * @param amount 揺れの強さ（0..1。1 でクラッシュくらい）
     */
    void shake(float amount) { m_shake = std::fmax(m_shake, amount); }

    /// 視点（揺れを含む）
    domain::Vec3 eye() const { return m_eye + m_shakeOffset; }

    /// 注視点（揺れを含む）
    domain::Vec3 look() const { return m_look + m_shakeOffset * 0.5f; }

    /// 今のモードの表示名
    const char* modeName() const;

    /// 縦方向の視野角（度）。速いほど、ドリフトが深いほど広がる
    float fovDegrees() const { return m_fovDegrees; }

private:
    static constexpr int MODE_COUNT = 3;

    /// カメラを壁の内側へ戻す
    void keepInside(const domain::Track& track, const domain::Car& car);

    domain::Vec3 m_eye{0, 8, -12};
    domain::Vec3 m_look{0, 0, 0};
    bool m_ready{};
    int m_mode{};
    float m_fovDegrees{60.0f};
    float m_shake{};
    float m_shakeTime{};
    domain::Vec3 m_shakeOffset;
};

}  // namespace adapters::gl
