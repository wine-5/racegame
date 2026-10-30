#pragma once

#include "adapters/gl/CpuMesh.hpp"
#include "domain/Car.hpp"

#include <deque>

namespace adapters::gl
{

/**
 * @brief 路面に残るタイヤ痕
 * @details 滑っている後輪の通った跡を細い帯で残す。古い跡から順に薄くなり、数が増えると消える。
 *          ドリフトの軌跡が見えるので、どんなラインで滑ったかがわかる。
 */
class SkidMarks
{
public:
    /**
     * @brief 跡を伸ばす
     * @param car 車の状態
     * @param emit 新しく跡を付けるか。ポーズ中は false にする
     * @return 跡が変わったら true（メッシュを作り直す必要がある）
     */
    bool update(const domain::Car& car, bool emit);

    /// 全部消す。リセットのあとに呼ぶ
    void clear();

    /**
     * @brief 路面の色を決める
     * @details 古い跡はこの色に近づいて消えていく。
     * @param color 路面の色
     */
    void setRoadColor(domain::Vec3 color) { m_roadColor = color; }

    /**
     * @brief 跡のメッシュを作る
     * @param mesh 作り先
     */
    void build(CpuMesh& mesh) const;

private:
    struct Segment
    {
        domain::Vec3 a0;  // 始まりの左右の端
        domain::Vec3 a1;
        domain::Vec3 b0;  // 終わりの左右の端
        domain::Vec3 b1;
        float darkness{};  // 0..1。濃いほど黒い
    };

    struct Wheel
    {
        bool active{};
        domain::Vec3 last;
        domain::Vec3 lastLeft;
        domain::Vec3 lastRight;
    };

    std::deque<Segment> m_segments;
    domain::Vec3 m_roadColor{0.17f, 0.17f, 0.19f};
    Wheel m_wheels[2];
};

}  // namespace adapters::gl
