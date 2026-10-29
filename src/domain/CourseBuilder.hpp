#pragma once

#include "domain/Track.hpp"

#include <string>
#include <vector>

namespace domain
{

/**
 * @brief 直線とカーブを順に並べてコースを組み立てる
 * @details 亀の歩き方（タートルグラフィックス）と同じで、今の位置と向きから部品の分だけ進む。
 *          道幅は部品ごとに決められ、部品の頭で前の幅からなめらかに変わる。
 *          最後の点が最初の点に戻るように部品を並べると、一周するコースになる。
 */
class CourseBuilder
{
public:
    /**
     * @brief 出発点を決めて作る
     * @param x 出発点の x
     * @param z 出発点の z
     * @param yaw 出発点の向き（ラジアン。0 で +z、右回りが正）
     * @param halfWidth 出発点の道幅の半分（m）
     */
    CourseBuilder(float x, float z, float yaw, float halfWidth);

    /**
     * @brief これから並べる部品に名前を付ける
     * @details HUD にコーナー名として出る。
     * @param name 区間の名前
     */
    void section(const std::string& name);

    /**
     * @brief 直線を足す
     * @param length 長さ（m）
     * @param halfWidth 道幅の半分（m）
     */
    void straight(float length, float halfWidth);

    /**
     * @brief 一定半径のカーブを足す
     * @param radius 半径（m）
     * @param degrees 曲がる角度（度）。右が正、左が負
     * @param halfWidth 道幅の半分（m）
     */
    void arc(float radius, float degrees, float halfWidth);

    /**
     * @brief 組み立てたコースを Track に書き込む
     * @details スタート位置は最初の点の少し手前にする。finalize も呼ぶ。
     * @param track 書き込み先
     * @return 点が足りないなど、コースとして使えなければ false
     */
    bool build(Track& track) const;

    /**
     * @brief 最後の点から最初の点までの距離を調べる
     * @return 一周のつなぎ目のずれ（m）。0 に近いほどきれいに閉じている
     */
    float closureGap() const;

private:
    /// 今の位置に点を 1 つ置く
    void addSample(float halfWidth);

    /// 部品の頭で幅を変えるときの、今の幅
    float blendWidth(float target, float travelled) const;

    float m_x{};
    float m_z{};
    float m_yaw{};
    float m_halfWidth{};
    float m_startX{};
    float m_startZ{};
    int m_nameId{-1};
    std::vector<std::string> m_names;
    std::vector<TrackSample> m_samples;
};

}  // namespace domain
