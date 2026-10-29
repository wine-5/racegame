#pragma once

#include "domain/Car.hpp"
#include "domain/Geom.hpp"
#include "domain/Track.hpp"

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

    /// 視点
    domain::Vec3 eye() const { return m_eye; }

    /// 注視点
    domain::Vec3 look() const { return m_look; }

    /// 今のモードの表示名
    const char* modeName() const;

private:
    static constexpr int MODE_COUNT = 3;

    domain::Vec3 m_eye{0, 8, -12};
    domain::Vec3 m_look{0, 0, 0};
    bool m_ready{};
    int m_mode{};
};

}  // namespace adapters::gl
