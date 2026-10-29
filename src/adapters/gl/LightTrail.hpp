#pragma once

#include "adapters/gl/Renderer.hpp"
#include "domain/Car.hpp"

#include <deque>
#include <vector>

namespace adapters::gl
{

/**
 * @brief ドリフト中にテールランプが残す赤い光の尾
 * @details 滑っている間のテールランプの位置を覚えておき、光る板を細かく並べて線のように描く。
 *          古い所から薄くなって消える。ブルームでにじみ、夜の路面に軌跡が浮かぶ。
 */
class LightTrail
{
public:
    /**
     * @brief テールランプの位置を記録し、古い記録を消す
     * @param car 車の状態
     * @param dt 前のフレームからの経過時間（秒）
     * @param emit 新しく記録するか。ポーズ中は false にする
     */
    void update(const domain::Car& car, float dt, bool emit);

    /// 全部消す
    void clear();

    /**
     * @brief 描くための板を追加する
     * @param out 追加先
     */
    void collect(std::vector<Billboard>& out) const;

private:
    struct Point
    {
        domain::Vec3 left;
        domain::Vec3 right;
        float age{};
        float strength{};  // 滑りの深さ（0..1）。深いほど太く明るい
        bool start{};      // 尾の書き始め（前の点とつながない）
    };

    std::deque<Point> m_points;
    bool m_active{};
};

}  // namespace adapters::gl
