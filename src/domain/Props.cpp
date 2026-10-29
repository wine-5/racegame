#include "domain/Props.hpp"

#include <cmath>

namespace domain
{

namespace
{

constexpr float CORNER_CURVATURE = 1.0f / 75.0f;  // これよりきついカーブに小物を置く（1/m）
constexpr float CONE_GROUP_SPACING = 40.0f;       // 長いカーブでは、この間隔で何組も置く（m）
constexpr float CONE_STEP = 3.5f;                 // 1 組のパイロンの間隔（m）
constexpr float CONE_INSET = 1.3f;                // 道の端からパイロンまで（m）
constexpr float DRUM_INSET = 1.0f;                // 道の端からドラム缶まで（m）
constexpr float DRUM_AFTER_EXIT = 6.0f;           // カーブの終わりからドラム缶まで（m）
constexpr float CONE_RADIUS = 0.3f;
constexpr float DRUM_RADIUS = 0.36f;
constexpr float CAR_RADIUS = 1.05f;  // 車を前・中・後ろの 3 つの円で近似する
constexpr float CAR_HALF_SPAN = 1.35f;
constexpr float GRAVITY = 9.8f;
constexpr float GROUND_FRICTION = 2.2f;  // 地面を滑るときの減り方（1/s）
constexpr float BOUNCE = 0.35f;
constexpr float HALF_PI = 1.5707963f;

/// 周回位置 s の点と向き（隣の点の間をつなぐ）
struct Spot
{
    float x{};
    float z{};
    float heading{};
    float halfWidth{};
};

Spot spotAt(const Track& track, float s)
{
    const auto& samples{track.samples};
    s = std::fmod(std::fmod(s, track.length) + track.length, track.length);
    std::size_t i{};
    while (i + 1 < samples.size() && samples[i + 1].s <= s)
    {
        ++i;
    }
    const TrackSample& a{samples[i]};
    const TrackSample& b{samples[(i + 1) % samples.size()]};
    float span{(i + 1 < samples.size() ? b.s : track.length) - a.s};
    float t{span > 1e-4f ? (s - a.s) / span : 0.0f};
    Spot spot;
    spot.x = a.x + (b.x - a.x) * t;
    spot.z = a.z + (b.z - a.z) * t;
    spot.heading = std::atan2(b.x - a.x, b.z - a.z);
    spot.halfWidth = a.halfWidth > 0.0f ? a.halfWidth : track.halfWidth;
    return spot;
}

/// 周回位置 s のカーブのきつさ（1/m）。右が正
float curvatureAt(const Track& track, float s)
{
    return wrapPi(spotAt(track, s + 4.0f).heading - spotAt(track, s - 4.0f).heading) / 8.0f;
}

/// 周回位置 s から横へずらした場所
Vec3 beside(const Track& track, float s, float lateral)
{
    Spot spot{spotAt(track, s)};
    return Vec3{spot.x + std::cos(spot.heading) * lateral, 0.0f, spot.z - std::sin(spot.heading) * lateral};
}

float radiusOf(PropKind kind)
{
    return kind == PropKind::Cone ? CONE_RADIUS : DRUM_RADIUS;
}

}  // namespace

void Props::place(const Track& track)
{
    m_props.clear();
    m_hits = 0;
    if (track.samples.size() < 3 || track.length <= 0.0f)
    {
        return;
    }
    auto add = [&](PropKind kind, Vec3 at)
    {
        Prop prop;
        prop.kind = kind;
        prop.home = at;
        prop.yaw = float(m_props.size()) * 0.7f;
        m_props.push_back(prop);
    };
    auto cones = [&](float s, float sign)
    {
        for (int i{-1}; i <= 1; ++i)
        {
            float at{s + float(i) * CONE_STEP};
            add(PropKind::Cone, beside(track, at, sign * (spotAt(track, at).halfWidth - CONE_INSET)));
        }
    };

    // カーブの区間を探す。始まりを直線の中から探して、1 周ぶん見る
    const float step{2.0f};
    float start{};
    while (start < track.length && std::fabs(curvatureAt(track, start)) > CORNER_CURVATURE)
    {
        start += step;
    }
    if (start >= track.length)
    {
        // 全部がカーブ（円の練習場）: 一定の間隔で内側にパイロン、外側にドラム缶
        float sign{curvatureAt(track, 0.0f) > 0 ? 1.0f : -1.0f};
        for (float s{}; s < track.length - CONE_GROUP_SPACING * 0.5f; s += CONE_GROUP_SPACING)
        {
            cones(s, sign);
            add(PropKind::Drum,
                beside(track, s + CONE_GROUP_SPACING * 0.5f, -sign * (spotAt(track, s).halfWidth - DRUM_INSET)));
        }
        reset();
        return;
    }
    bool inCorner{};
    float nextGroup{};
    float cornerSign{1.0f};
    for (float d{}; d <= track.length; d += step)
    {
        float s{start + d};
        float k{curvatureAt(track, s)};
        bool curved{std::fabs(k) > CORNER_CURVATURE};
        if (curved && !inCorner)
        {
            inCorner = true;
            cornerSign = k > 0 ? 1.0f : -1.0f;
            nextGroup = s + 10.0f;
        }
        else if (curved && (k > 0 ? 1.0f : -1.0f) != cornerSign)
        {
            // S 字で向きが変わった: 前のカーブを閉じて、新しいカーブとして数える
            add(PropKind::Drum, beside(track, s, cornerSign * -(spotAt(track, s).halfWidth - DRUM_INSET)));
            cornerSign = -cornerSign;
            nextGroup = s + 10.0f;
        }
        else if (!curved && inCorner)
        {
            inCorner = false;
            // 出口の外側にドラム缶を 2 つ
            for (int i{}; i < 2; ++i)
            {
                float at{s + DRUM_AFTER_EXIT + float(i) * 2.2f};
                add(PropKind::Drum, beside(track, at, -cornerSign * (spotAt(track, at).halfWidth - DRUM_INSET)));
            }
            continue;
        }
        if (inCorner && s >= nextGroup)
        {
            cones(s, cornerSign);
            nextGroup = s + CONE_GROUP_SPACING;
        }
    }
    reset();
}

void Props::reset()
{
    for (Prop& prop : m_props)
    {
        PropKind kind{prop.kind};
        Vec3 home{prop.home};
        float yaw{prop.yaw};
        prop = Prop{};
        prop.kind = kind;
        prop.home = home;
        prop.position = home;
        prop.yaw = yaw;
    }
}

bool Props::collide(Prop& prop, const Car& car)
{
    Vec3 forward{std::sin(car.yaw), 0.0f, std::cos(car.yaw)};
    Vec3 right{std::cos(car.yaw), 0.0f, -std::sin(car.yaw)};
    Vec3 center{car.x, 0.0f, car.z};
    float reach{CAR_RADIUS + radiusOf(prop.kind)};
    for (int i{-1}; i <= 1; ++i)
    {
        Vec3 circle{center + forward * (float(i) * CAR_HALF_SPAN)};
        Vec3 away{prop.position - circle};
        away.y = 0;
        float distance{length(away)};
        if (distance > reach || prop.position.y > 1.2f)
        {
            continue;
        }
        Vec3 carVelocity{forward * car.speed + right * car.lateralSpeed};
        float speed{length(carVelocity)};
        Vec3 push{distance > 1e-3f ? away * (1.0f / distance) : forward};
        // 飛ばす向き: 車の速度に、当たった向きへの押しを足す。軽いパイロンほど高く飛ぶ
        bool cone{prop.kind == PropKind::Cone};
        m_seed = m_seed * 1664525u + 1013904223u;
        float random{float(m_seed >> 8) / float(1u << 24) * 2.0f - 1.0f};
        prop.velocity = carVelocity * (cone ? 1.15f : 0.85f) + push * (cone ? 4.0f : 2.5f);
        prop.velocity.y = (cone ? 3.0f : 1.6f) + speed * (cone ? 0.12f : 0.06f);
        prop.spin = (cone ? 9.0f : 5.0f) * (random > 0 ? 1.0f : -1.0f) + random * 3.0f;
        prop.tiltRate = cone ? 9.0f : 5.0f;
        prop.position = circle + push * (reach + 0.02f);
        prop.position.y = std::fmax(prop.position.y, 0.0f);
        bool first{!prop.knocked};
        prop.knocked = true;
        m_lastHitPosition = prop.position;
        m_lastHitKind = prop.kind;
        m_lastHitSpeed = speed;
        return first;
    }
    return false;
}

void Props::update(const Car& car, float dt)
{
    for (Prop& prop : m_props)
    {
        if (collide(prop, car))
        {
            ++m_hits;
        }
        if (!prop.knocked)
        {
            continue;
        }
        prop.velocity.y -= GRAVITY * dt;
        prop.position = prop.position + prop.velocity * dt;
        prop.yaw += prop.spin * dt;
        prop.tilt = std::fmin(prop.tilt + prop.tiltRate * dt, HALF_PI);
        if (prop.position.y <= 0.0f)
        {
            prop.position.y = 0.0f;
            prop.velocity.y = prop.velocity.y < -1.5f ? -prop.velocity.y * BOUNCE : 0.0f;
            // 地面を滑りながら止まる
            float keep{std::exp(-GROUND_FRICTION * dt)};
            prop.velocity.x *= keep;
            prop.velocity.z *= keep;
            prop.spin *= keep;
        }
    }
}

}  // namespace domain
