#pragma once

#include "domain/Car.hpp"
#include "domain/Track.hpp"

#include <vector>

namespace domain
{

/**
 * @brief コースに置く小物の種類
 */
enum class PropKind
{
    Cone,  // パイロン。コーナーの内側（クリッピングポイント）に置く
    Drum   // ドラム缶。コーナーの出口の外側に置く
};

/**
 * @brief 弾き飛ばせる小物 1 つ
 */
struct Prop
{
    PropKind kind = PropKind::Cone;
    Vec3 home;         // 置いた場所
    Vec3 position;     // 今の場所（底の中心）
    Vec3 velocity;     // m/s
    float yaw{};       // 向き（ラジアン）
    float spin{};      // 向きの変わる速さ（rad/s）
    float tilt{};      // 倒れている角度（ラジアン）。π/2 で横倒し
    float tiltRate{};  // 倒れる速さ（rad/s）
    bool knocked{};    // 弾かれたか
};

/**
 * @brief 車で弾き飛ばせるパイロンとドラム缶
 * @details コーナーの内側にパイロン、出口の外側にドラム缶を置く。車が当たると飛んで転がる。
 *          車の動きには影響しない。周回が変わると元の場所に戻る。
 */
class Props
{
public:
    /**
     * @brief コースの形から小物を置く
     * @param track 置くコース
     */
    void place(const Track& track);

    /// 全部元の場所に戻す
    void reset();

    /**
     * @brief 1 ステップ分進める
     * @param car 車の状態
     * @param dt 進める時間（秒）
     */
    void update(const Car& car, float dt);

    /// 小物の並び
    const std::vector<Prop>& props() const { return m_props; }

    /// これまでに初めて弾いた数。増えたかどうかで当たった瞬間を知る
    int hits() const { return m_hits; }

    /// 最後に当たった場所
    Vec3 lastHitPosition() const { return m_lastHitPosition; }

    /// 最後に当たった小物の種類
    PropKind lastHitKind() const { return m_lastHitKind; }

    /// 最後に当たったときの車の速さ（m/s）
    float lastHitSpeed() const { return m_lastHitSpeed; }

private:
    /// 車に押されたら飛ばす。初めて弾いたら true
    bool collide(Prop& prop, const Car& car);

    std::vector<Prop> m_props;
    int m_hits{};
    Vec3 m_lastHitPosition;
    PropKind m_lastHitKind = PropKind::Cone;
    float m_lastHitSpeed{};
    unsigned m_seed{12345u};
};

}  // namespace domain
