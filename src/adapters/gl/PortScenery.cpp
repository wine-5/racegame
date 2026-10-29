#include "adapters/gl/PortScenery.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

namespace adapters::gl
{

using domain::cross;
using domain::length;
using domain::normalize;
using domain::Track;
using domain::TrackSample;
using domain::Vec3;

namespace
{

const Vec3 ROAD{0.15f, 0.155f, 0.17f};
const Vec3 CONCRETE{0.20f, 0.20f, 0.22f};
const Vec3 GROUND{0.08f, 0.085f, 0.095f};
const Vec3 SEA{0.012f, 0.03f, 0.06f};
const Vec3 LINE_YELLOW{0.85f, 0.62f, 0.08f};
const Vec3 HAZARD_BLACK{0.03f, 0.03f, 0.035f};
const Vec3 SODIUM{1.0f, 0.70f, 0.40f};
const Vec3 CYAN{0.15f, 0.85f, 1.0f};
const Vec3 MAGENTA{1.0f, 0.18f, 0.65f};
const Vec3 UP{0, 1, 0};

const Vec3 CONTAINER_COLORS[]{
    {0.55f, 0.14f, 0.10f},  // 錆びた赤
    {0.10f, 0.22f, 0.48f},  // 紺
    {0.12f, 0.40f, 0.22f},  // 緑
    {0.85f, 0.42f, 0.08f},  // 橙
    {0.05f, 0.42f, 0.44f},  // 青緑
    {0.58f, 0.58f, 0.60f},  // 灰
    {0.42f, 0.08f, 0.14f},  // えんじ
    {0.85f, 0.66f, 0.10f},  // 黄
};

constexpr float CONTAINER_HEIGHT = 2.6f;
constexpr float CONTAINER_DEPTH = 2.44f;
constexpr float CONTAINER_SHORT = 6.06f;
constexpr float CONTAINER_LONG = 12.19f;
constexpr float LAMP_SPACING = 30.0f;
constexpr float LAMP_HEIGHT = 8.5f;
constexpr float LAMP_RADIUS = 34.0f;
constexpr float NEON_SPACING = 22.0f;

/// 毎回同じ並びになる乱数
class Random
{
public:
    explicit Random(std::uint32_t seed) : m_seed(seed) {}

    /// 0..1 の乱数
    float next()
    {
        m_seed = m_seed * 1664525u + 1013904223u;
        return float(m_seed >> 8) / float(1u << 24);
    }

    /// 0..count-1 の整数
    int pick(int count) { return std::min(count - 1, static_cast<int>(next() * float(count))); }

private:
    std::uint32_t m_seed;
};

/// コースの点 1 つ分の位置と向き
struct Frame
{
    Vec3 position;
    Vec3 forward;
    Vec3 right;
    float halfWidth{};
    float wall{};  // 中心線から壁までの距離
    float s{};
    bool quayside{};
    bool alley{};
    bool plaza{};  // クレーン広場のヘアピン
};

std::vector<Frame> makeFrames(const Track& track)
{
    std::vector<Frame> frames;
    const int n{static_cast<int>(track.samples.size())};
    for (int i{}; i < n; ++i)
    {
        const TrackSample& prev{track.samples[std::size_t((i + n - 1) % n)]};
        const TrackSample& next{track.samples[std::size_t((i + 1) % n)]};
        const TrackSample& sample{track.samples[std::size_t(i)]};
        Frame frame;
        frame.position = {sample.x, sample.y, sample.z};
        frame.forward = normalize(Vec3{next.x - prev.x, 0, next.z - prev.z});
        frame.right = {frame.forward.z, 0, -frame.forward.x};
        frame.halfWidth = sample.halfWidth;
        frame.wall = track.wallLateral(sample.halfWidth);
        frame.s = sample.s;
        std::string name{track.nameOf(sample.nameId)};
        frame.quayside = name == "QUAYSIDE";
        frame.alley = name == "CONTAINER ALLEY";
        frame.plaza = name == "CRANE HAIRPIN";
        frames.push_back(frame);
    }
    return frames;
}

Vec3 side(const Frame& frame, float lateral, float y)
{
    Vec3 p{frame.position + frame.right * lateral};
    p.y += y;
    return p;
}

/// 中心線に沿った帯。lat0 から lat1 まで（右が正）
template <class LatA, class LatB, class Color>
void addStrip(CpuMesh& mesh, const std::vector<Frame>& frames, LatA lat0, LatB lat1, Color color, float y)
{
    const std::size_t n{frames.size()};
    for (std::size_t i{}; i < n; ++i)
    {
        std::size_t j{(i + 1) % n};
        Vec3 a{side(frames[i], lat0(frames[i]), y)};
        Vec3 b{side(frames[i], lat1(frames[i]), y)};
        Vec3 c{side(frames[j], lat1(frames[j]), y)};
        Vec3 d{side(frames[j], lat0(frames[j]), y)};
        mesh.addQuad(a, b, c, d, UP, color(i));
    }
}

/// 向きを決めた箱。axisX・axisY・axisZ は半分の長さを持つ軸
void addOrientedBox(CpuMesh& mesh, Vec3 center, Vec3 axisX, Vec3 axisY, Vec3 axisZ, Vec3 color)
{
    Vec3 p[8];
    for (int i{}; i < 8; ++i)
    {
        p[i] = center + axisX * ((i & 1) ? 1.0f : -1.0f) + axisY * ((i & 2) ? 1.0f : -1.0f) +
               axisZ * ((i & 4) ? 1.0f : -1.0f);
    }
    Vec3 nx{normalize(axisX)};
    Vec3 ny{normalize(axisY)};
    Vec3 nz{normalize(axisZ)};
    mesh.addQuad(p[2], p[3], p[7], p[6], ny, color * 1.08f);
    mesh.addQuad(p[0], p[1], p[5], p[4], ny * -1.0f, color);
    mesh.addQuad(p[1], p[3], p[7], p[5], nx, color);
    mesh.addQuad(p[0], p[2], p[6], p[4], nx * -1.0f, color);
    mesh.addQuad(p[4], p[5], p[7], p[6], nz, color);
    mesh.addQuad(p[0], p[1], p[3], p[2], nz * -1.0f, color);
}

/// コンテナ 1 段。長い面に縦のうねり（明るい筋と暗い筋）を付ける
void addContainer(CpuMesh& mesh, Vec3 base, Vec3 along, Vec3 outward, float lengthM, Vec3 color)
{
    const float half{lengthM * 0.5f};
    const float depth{CONTAINER_DEPTH * 0.5f};
    Vec3 center{base + Vec3{0, CONTAINER_HEIGHT * 0.5f, 0}};
    addOrientedBox(mesh, center, along * half, Vec3{0, CONTAINER_HEIGHT * 0.5f, 0}, outward * depth, color * 0.8f);
    // 道に面した側に、うねりの筋を少し浮かせて貼る
    const int ribs{static_cast<int>(lengthM / 0.6f)};
    Vec3 face{center - outward * (depth + 0.02f)};
    for (int i{}; i < ribs; ++i)
    {
        float t0{-half + lengthM * float(i) / float(ribs)};
        float t1{-half + lengthM * float(i + 1) / float(ribs)};
        float shade{(i % 2 == 0) ? 1.0f : 0.78f};
        Vec3 bottom{0, -CONTAINER_HEIGHT * 0.5f + 0.08f, 0};
        Vec3 top{0, CONTAINER_HEIGHT * 0.5f - 0.08f, 0};
        mesh.addQuad(face + along * t0 + bottom, face + along * t1 + bottom, face + along * t1 + top,
                     face + along * t0 + top, outward * -1.0f, color * shade);
    }
}

/// 壁に沿ってコンテナを並べる。side は -1 が左、1 が右
void addContainerWall(CpuMesh& mesh, const std::vector<Frame>& frames, float sideSign, Random& random,
                      std::vector<PointLight>& lamps)
{
    const std::size_t n{frames.size()};
    float carry{};
    int neonIndex{};
    for (std::size_t i{}; i < n; ++i)
    {
        const Frame& frame{frames[i]};
        const Frame& next{frames[(i + 1) % n]};
        Vec3 wall0{side(frame, sideSign * frame.wall, 0)};
        Vec3 wall1{side(next, sideSign * next.wall, 0)};
        float step{length(wall1 - wall0)};
        // 海沿いの左と、広場の内側（照明塔が見えるように）は低い柵にする
        if ((frame.quayside && sideSign < 0) || (frame.plaza && sideSign > 0))
        {
            carry = 0;
            continue;
        }
        if (carry > 0)
        {
            carry -= step;
            continue;
        }
        // 曲がりがきつい所は短いコンテナにする。長い箱だと内側で道にはみ出すため
        float turn{
            std::fabs(std::atan2(cross(frame.forward, next.forward).y, domain::dot(frame.forward, next.forward)))};
        float radius{turn > 1e-4f ? step / turn : 1e9f};
        float lengthM{(radius < 70.0f || random.next() < 0.5f) ? CONTAINER_SHORT : CONTAINER_LONG};
        Vec3 along{normalize(wall1 - wall0)};
        Vec3 outward{frame.right * sideSign};
        float margin{radius < 70.0f ? 0.35f : 0.05f};
        Vec3 base{wall0 + along * (lengthM * 0.5f) + outward * (CONTAINER_DEPTH * 0.5f + margin)};
        int tiers{frame.alley ? 2 + random.pick(2) : 1 + random.pick(2)};
        for (int tier{}; tier < tiers; ++tier)
        {
            Vec3 color{CONTAINER_COLORS[random.pick(8)]};
            addContainer(mesh, base + Vec3{0, CONTAINER_HEIGHT * float(tier), 0}, along, outward, lengthM, color);
        }
        // コンテナの間の道は、上の段にネオンの帯を付けて色の光を落とす
        if (frame.alley)
        {
            Vec3 neonColor{(neonIndex % 2 == 0) ? CYAN : MAGENTA};
            ++neonIndex;
            float topY{CONTAINER_HEIGHT * float(tiers) - 0.25f};
            Vec3 face{base - outward * (CONTAINER_DEPTH * 0.5f + 0.06f) + Vec3{0, topY, 0}};
            Vec3 h{along * (lengthM * 0.48f)};
            mesh.addQuad(face - h, face + h, face + h + Vec3{0, 0.18f, 0}, face - h + Vec3{0, 0.18f, 0},
                         outward * -1.0f, glow(neonColor));
            if (neonIndex % 2 == 0)
            {
                lamps.push_back({face - outward * 1.5f, neonColor * 1.1f, 16.0f});
            }
        }
        carry = lengthM + 0.25f - step;
    }
}

/// 低い防護柵（黄と黒の縞）。sideSign は -1 が左、1 が右。seaDrop なら外側を海へ落ちる岸壁にする
template <class Where>
void addBarrier(CpuMesh& mesh, const std::vector<Frame>& frames, float sideSign, Where where, bool seaDrop)
{
    const std::size_t n{frames.size()};
    for (std::size_t i{}; i < n; ++i)
    {
        const Frame& a{frames[i]};
        const Frame& b{frames[(i + 1) % n]};
        if (!where(a) || !where(b))
        {
            continue;
        }
        Vec3 a0{side(a, sideSign * a.wall, 0)};
        Vec3 b0{side(b, sideSign * b.wall, 0)};
        Vec3 a1{side(a, sideSign * (a.wall + 0.5f), 0)};
        Vec3 b1{side(b, sideSign * (b.wall + 0.5f), 0)};
        Vec3 h{0, 0.9f, 0};
        bool stripe{static_cast<int>(a.s / 2.0f) % 2 == 0};
        Vec3 color{stripe ? LINE_YELLOW : HAZARD_BLACK};
        mesh.addQuad(a0, b0, b0 + h, a0 + h, a.right * -sideSign, color);
        mesh.addQuad(a0 + h, b0 + h, b1 + h, a1 + h, UP, CONCRETE);
        if (seaDrop)
        {
            // 岸壁の切り立った面（海へ落ちる）
            Vec3 drop{0, -2.5f, 0};
            mesh.addQuad(a1, b1, b1 + drop, a1 + drop, a.right * sideSign, CONCRETE * 0.7f);
        }
    }
}

/// 街灯 1 本。柱は壁の外に立ち、腕を道の上へ伸ばす
void addLamp(CpuMesh& mesh, const Frame& frame, float sideSign, std::vector<PointLight>& lamps)
{
    Vec3 outward{frame.right * sideSign};
    Vec3 foot{side(frame, sideSign * (frame.wall + CONTAINER_DEPTH + 0.6f), 0)};
    const Vec3 pole{0.26f, 0.27f, 0.30f};
    addOrientedBox(mesh, foot + Vec3{0, LAMP_HEIGHT * 0.5f, 0}, frame.forward * 0.15f, Vec3{0, LAMP_HEIGHT * 0.5f, 0},
                   outward * 0.15f, pole);
    float reach{CONTAINER_DEPTH + 2.2f};
    Vec3 arm{foot - outward * (reach * 0.5f) + Vec3{0, LAMP_HEIGHT, 0}};
    addOrientedBox(mesh, arm, frame.forward * 0.1f, Vec3{0, 0.1f, 0}, outward * (reach * 0.5f), pole);
    Vec3 head{foot - outward * reach + Vec3{0, LAMP_HEIGHT - 0.15f, 0}};
    addOrientedBox(mesh, head, frame.forward * 0.45f, Vec3{0, 0.1f, 0}, outward * 0.3f, glow(Vec3{1.0f, 0.86f, 0.62f}));
    lamps.push_back({head - Vec3{0, 0.6f, 0}, SODIUM * 2.4f, LAMP_RADIUS});
}

/// 高い照明塔。広場の真ん中に立てて広く照らす
void addFloodTower(CpuMesh& mesh, Vec3 base, float height, std::vector<PointLight>& lamps)
{
    const Vec3 steel{0.22f, 0.23f, 0.26f};
    addOrientedBox(mesh, base + Vec3{0, height * 0.5f, 0}, Vec3{0.5f, 0, 0}, Vec3{0, height * 0.5f, 0},
                   Vec3{0, 0, 0.5f}, steel);
    addOrientedBox(mesh, base + Vec3{0, height, 0}, Vec3{2.2f, 0, 0}, Vec3{0, 0.6f, 0}, Vec3{0, 0, 2.2f},
                   glow(Vec3{1.0f, 0.95f, 0.85f}));
    lamps.push_back({base + Vec3{0, height - 2.0f, 0}, Vec3{1.0f, 0.92f, 0.8f} * 3.0f, 160.0f});
}

/// 岸壁のガントリークレーン。道をまたいで立ち、海へ腕を突き出す。てっぺんに赤い航空障害灯
void addCrane(CpuMesh& mesh, Vec3 base, Vec3 along, Vec3 toSea, float gauge, Random& random)
{
    const Vec3 colors[]{{0.75f, 0.30f, 0.06f}, {0.12f, 0.30f, 0.55f}};
    Vec3 color{colors[random.pick(2)]};
    const float height{34.0f};
    const float span{16.0f};
    for (int i{}; i < 4; ++i)
    {
        Vec3 leg{base + along * ((i & 1) ? span * 0.5f : -span * 0.5f) +
                 toSea * ((i & 2) ? gauge * 0.5f : -gauge * 0.5f)};
        addOrientedBox(mesh, leg + Vec3{0, height * 0.5f, 0}, along * 0.6f, Vec3{0, height * 0.5f, 0}, toSea * 0.6f,
                       color);
    }
    Vec3 top{base + Vec3{0, height, 0}};
    addOrientedBox(mesh, top, along * (span * 0.5f + 0.6f), Vec3{0, 1.6f, 0}, toSea * (gauge * 0.5f + 0.6f), color);
    // 海へ突き出す腕
    Vec3 boom{top + toSea * 26.0f + Vec3{0, 2.6f, 0}};
    addOrientedBox(mesh, boom, along * 1.4f, Vec3{0, 1.0f, 0}, toSea * 34.0f, color * 0.9f);
    Vec3 machine{top + Vec3{0, 4.5f, 0} - toSea * 2.0f};
    addOrientedBox(mesh, machine, along * 3.0f, Vec3{0, 2.0f, 0}, toSea * 4.0f, Vec3{0.8f, 0.8f, 0.78f});
    addOrientedBox(mesh, machine + Vec3{0, 2.4f, 0}, Vec3{0.3f, 0, 0}, Vec3{0, 0.3f, 0}, Vec3{0, 0, 0.3f},
                   glow(Vec3{1.0f, 0.1f, 0.05f}));
    addOrientedBox(mesh, boom + toSea * 33.0f + Vec3{0, 1.3f, 0}, Vec3{0.3f, 0, 0}, Vec3{0, 0.3f, 0}, Vec3{0, 0, 0.3f},
                   glow(Vec3{1.0f, 0.1f, 0.05f}));
}

/// 倉庫。壁に窓の灯りの列と、ネオンの看板を付ける
void addWarehouse(CpuMesh& mesh, Vec3 center, Vec3 along, Vec3 outward, Random& random)
{
    float lengthM{36.0f + random.next() * 30.0f};
    float depth{24.0f + random.next() * 12.0f};
    float height{11.0f + random.next() * 7.0f};
    const Vec3 wallColor{0.20f + random.next() * 0.06f, 0.21f, 0.24f};
    Vec3 c{center + outward * (depth * 0.5f) + Vec3{0, height * 0.5f, 0}};
    addOrientedBox(mesh, c, along * (lengthM * 0.5f), Vec3{0, height * 0.5f, 0}, outward * (depth * 0.5f), wallColor);
    Vec3 face{c - outward * (depth * 0.5f + 0.05f)};
    // 窓の灯り
    int windows{static_cast<int>(lengthM / 4.0f)};
    for (int i{}; i < windows; ++i)
    {
        if (random.next() < 0.35f)
        {
            continue;
        }
        float t{-lengthM * 0.5f + (float(i) + 0.5f) * lengthM / float(windows)};
        Vec3 w{face + along * t + Vec3{0, height * 0.18f, 0}};
        Vec3 h{along * 1.1f};
        Vec3 v{0, 0.7f, 0};
        mesh.addQuad(w - h - v, w + h - v, w + h + v, w - h + v, outward * -1.0f, glow(Vec3{0.9f, 0.7f, 0.35f} * 0.8f));
    }
    // ネオンの看板
    Vec3 signColor{random.next() < 0.5f ? CYAN : MAGENTA};
    Vec3 sign{face + Vec3{0, height * 0.72f, 0} - outward * 0.05f};
    Vec3 sh{along * (lengthM * 0.3f)};
    Vec3 sv{0, 0.55f, 0};
    mesh.addQuad(sign - sh - sv, sign + sh - sv, sign + sh + sv, sign - sh + sv, outward * -1.0f, glow(signColor));
}

/// 対岸の街の灯り。海の向こうに光る点を並べ、海面に縦の映り込みを描く
void addFarShore(CpuMesh& mesh, float seaX, float minZ, float maxZ, Random& random)
{
    const float shoreX{seaX - 900.0f};
    for (float z{minZ - 600.0f}; z < maxZ + 600.0f; z += 9.0f + random.next() * 14.0f)
    {
        float height{6.0f + random.next() * 50.0f};
        float width{6.0f + random.next() * 10.0f};
        Vec3 base{shoreX - random.next() * 80.0f, -1.0f, z};
        addOrientedBox(mesh, base + Vec3{0, height * 0.5f, 0}, Vec3{width * 0.5f, 0, 0}, Vec3{0, height * 0.5f, 0},
                       Vec3{0, 0, width * 0.5f}, Vec3{0.03f, 0.035f, 0.05f});
        int lights{static_cast<int>(height / 4.0f)};
        for (int i{}; i < lights; ++i)
        {
            if (random.next() < 0.5f)
            {
                continue;
            }
            Vec3 color{random.next() < 0.75f ? Vec3{1.0f, 0.78f, 0.45f} : Vec3{0.6f, 0.85f, 1.0f}};
            Vec3 p{base.x + width * 0.5f + 0.1f, -1.0f + 2.0f + float(i) * 4.0f,
                   z + (random.next() - 0.5f) * width * 0.6f};
            Vec3 a{0, 0, 0.9f};
            Vec3 b{0, 0.7f, 0};
            mesh.addQuad(p - a - b, p + a - b, p + a + b, p - a + b, Vec3{1, 0, 0}, glow(color * 0.9f));
            // 海面への映り込み
            if (i < 3)
            {
                Vec3 r{shoreX + 120.0f + random.next() * 300.0f, -1.15f, p.z};
                Vec3 ra{0, 0, 0.7f};
                Vec3 rb{random.next() * 40.0f + 20.0f, 0, 0};
                mesh.addQuad(r - ra, r + ra, r + ra + rb, r - ra + rb, UP, glow(color * 0.12f));
            }
        }
    }
}

/// スタートの門。光る梁と、市松模様のライン
void addStartGate(CpuMesh& mesh, const Frame& frame)
{
    const int cells{12};
    for (int i{}; i < cells; ++i)
    {
        float l0{-frame.halfWidth + 2.0f * frame.halfWidth * float(i) / float(cells)};
        float l1{-frame.halfWidth + 2.0f * frame.halfWidth * float(i + 1) / float(cells)};
        for (int row{}; row < 2; ++row)
        {
            Vec3 color{((i + row) % 2 == 0) ? Vec3{0.85f, 0.85f, 0.88f} : Vec3{0.03f, 0.03f, 0.035f}};
            Vec3 f0{frame.forward * (0.6f * float(row))};
            Vec3 f1{frame.forward * (0.6f * float(row + 1))};
            mesh.addQuad(side(frame, l0, 0.03f) + f0, side(frame, l1, 0.03f) + f0, side(frame, l1, 0.03f) + f1,
                         side(frame, l0, 0.03f) + f1, UP, color);
        }
    }
    const Vec3 steel{0.2f, 0.21f, 0.24f};
    float post{frame.wall + 0.5f};
    for (int s{-1}; s <= 1; s += 2)
    {
        Vec3 foot{side(frame, float(s) * post, 0)};
        addOrientedBox(mesh, foot + Vec3{0, 3.5f, 0}, frame.forward * 0.3f, Vec3{0, 3.5f, 0}, frame.right * 0.3f,
                       steel);
    }
    Vec3 beam{frame.position + Vec3{0, 7.2f, 0}};
    addOrientedBox(mesh, beam, frame.forward * 0.4f, Vec3{0, 0.5f, 0}, frame.right * post, steel);
    addOrientedBox(mesh, beam - frame.forward * 0.42f, frame.forward * 0.02f, Vec3{0, 0.25f, 0},
                   frame.right * (post - 0.6f), glow(MAGENTA));
}

}  // namespace

Lighting portLighting()
{
    Lighting lighting;
    lighting.sunDir = {-0.4f, 0.75f, 0.5f};
    lighting.sunColor = {0.10f, 0.12f, 0.19f};
    lighting.ambient = {0.09f, 0.10f, 0.15f};
    lighting.fogColor = {0.025f, 0.035f, 0.07f};
    lighting.fogStart = 60.0f;
    lighting.fogRange = 900.0f;
    lighting.skyHorizon = {0.06f, 0.07f, 0.13f};
    lighting.skyZenith = {0.004f, 0.008f, 0.025f};
    return lighting;
}

domain::Vec3 portRoadColor()
{
    return ROAD;
}

void buildPortScenery(const Track& track, CpuMesh& mesh, std::vector<PointLight>& lamps)
{
    mesh = CpuMesh{};
    lamps.clear();
    std::vector<Frame> frames{makeFrames(track)};
    if (frames.size() < 3)
    {
        return;
    }
    Random random{20261029u};

    // コースの範囲と、海との境目（海沿いの区間の左の壁）
    float minX{1e9f};
    float maxX{-1e9f};
    float minZ{1e9f};
    float maxZ{-1e9f};
    float seaX{1e9f};
    bool hasSea{};
    for (const Frame& frame : frames)
    {
        minX = std::min(minX, frame.position.x);
        maxX = std::max(maxX, frame.position.x);
        minZ = std::min(minZ, frame.position.z);
        maxZ = std::max(maxZ, frame.position.z);
        if (frame.quayside)
        {
            hasSea = true;
            seaX = std::min(seaX, side(frame, -frame.wall - 0.5f, 0).x);
        }
    }
    const float pad{260.0f};
    float groundLeft{hasSea ? seaX : minX - pad};
    // 地面（港の舗装）と海
    mesh.addQuad({groundLeft, -0.02f, minZ - pad}, {maxX + pad, -0.02f, minZ - pad}, {maxX + pad, -0.02f, maxZ + pad},
                 {groundLeft, -0.02f, maxZ + pad}, UP, GROUND);
    if (hasSea)
    {
        mesh.addQuad({seaX - 2000.0f, -1.2f, minZ - 1500.0f}, {seaX, -1.2f, minZ - 1500.0f},
                     {seaX, -1.2f, maxZ + 1500.0f}, {seaX - 2000.0f, -1.2f, maxZ + 1500.0f}, UP, SEA);
        addFarShore(mesh, seaX, minZ, maxZ, random);
    }

    // 路面。少しずつ明るさを変えて、のっぺりしないようにする
    addStrip(
        mesh, frames, [](const Frame& f) { return -f.halfWidth; }, [](const Frame& f) { return f.halfWidth; },
        [](std::size_t i) { return ROAD * (0.92f + 0.12f * float((i * 7) % 5) / 5.0f); }, 0.0f);
    // 黄色の縁取りの線
    for (float s{-1.0f}; s <= 1.0f; s += 2.0f)
    {
        addStrip(
            mesh, frames, [s](const Frame& f) { return s * (f.halfWidth - 0.55f); },
            [s](const Frame& f) { return s * (f.halfWidth - 0.3f); }, [](std::size_t) { return LINE_YELLOW; }, 0.015f);
        // 縁石の代わりの、黄と黒の縞
        addStrip(
            mesh, frames, [s](const Frame& f) { return s * f.halfWidth; },
            [s, &track](const Frame& f) { return s * (f.halfWidth + track.curbWidth); }, [&frames](std::size_t i)
            { return (static_cast<int>(frames[i].s / 1.5f) % 2 == 0) ? LINE_YELLOW * 0.8f : HAZARD_BLACK; }, 0.01f);
        // 縁石の外から壁までのコンクリート
        addStrip(
            mesh, frames, [s, &track](const Frame& f) { return s * (f.halfWidth + track.curbWidth); },
            [s](const Frame& f) { return s * f.wall; }, [](std::size_t) { return CONCRETE; }, 0.005f);
    }

    // 壁: 右と左にコンテナ。海沿いの左は低い柵
    addContainerWall(mesh, frames, 1.0f, random, lamps);
    addContainerWall(mesh, frames, -1.0f, random, lamps);
    if (hasSea)
    {
        addBarrier(mesh, frames, -1.0f, [](const Frame& f) { return f.quayside; }, true);
        // 海沿いの直線に、道をまたぐクレーンを並べる
        float nextCrane{50.0f};
        for (const Frame& frame : frames)
        {
            if (!frame.quayside || frame.s < nextCrane)
            {
                continue;
            }
            nextCrane = frame.s + 62.0f;
            float left{frame.wall + 1.2f};
            float right{frame.wall + CONTAINER_DEPTH + 1.5f};
            Vec3 base{side(frame, (right - left) * 0.5f, 0)};
            addCrane(mesh, base, frame.forward, frame.right * -1.0f, left + right, random);
        }
    }

    // 街灯。左右交互に並べる
    float nextLamp{};
    float sideSign{1.0f};
    for (const Frame& frame : frames)
    {
        if (frame.s < nextLamp)
        {
            continue;
        }
        nextLamp = frame.s + LAMP_SPACING;
        sideSign = -sideSign;
        if (frame.quayside && sideSign < 0)
        {
            // 海側は柵の上に立てる
            Vec3 foot{side(frame, -frame.wall - 0.25f, 0.9f)};
            addOrientedBox(mesh, foot + Vec3{0, LAMP_HEIGHT * 0.5f, 0}, frame.forward * 0.15f,
                           Vec3{0, LAMP_HEIGHT * 0.5f, 0}, frame.right * 0.15f, Vec3{0.26f, 0.27f, 0.3f});
            Vec3 head{foot + frame.right * 1.6f + Vec3{0, LAMP_HEIGHT - 0.15f, 0}};
            addOrientedBox(mesh, foot + frame.right * 0.8f + Vec3{0, LAMP_HEIGHT, 0}, frame.forward * 0.1f,
                           Vec3{0, 0.1f, 0}, frame.right * 0.8f, Vec3{0.26f, 0.27f, 0.3f});
            addOrientedBox(mesh, head, frame.forward * 0.45f, Vec3{0, 0.1f, 0}, frame.right * 0.3f,
                           glow(Vec3{1.0f, 0.86f, 0.62f}));
            lamps.push_back({head - Vec3{0, 0.6f, 0}, SODIUM * 2.4f, LAMP_RADIUS});
            continue;
        }
        addLamp(mesh, frame, sideSign, lamps);
    }

    // 区間ごとの目印
    std::size_t hairpinFirst{frames.size()};
    std::size_t hairpinLast{};
    for (std::size_t i{}; i < frames.size(); ++i)
    {
        if (track.nameOf(track.samples[i].nameId) == std::string{"CRANE HAIRPIN"})
        {
            hairpinFirst = std::min(hairpinFirst, i);
            hairpinLast = std::max(hairpinLast, i);
        }
    }
    if (hairpinFirst < hairpinLast)
    {
        // ヘアピンの円の中心に照明塔を立てて、広場全体を照らす。内側は低い柵なので塔がよく見える
        addBarrier(mesh, frames, 1.0f, [](const Frame& f) { return f.plaza; }, false);
        Vec3 center{(frames[hairpinFirst].position + frames[hairpinLast].position) * 0.5f};
        center.y = 0;
        addFloodTower(mesh, center, 30.0f, lamps);
    }
    else if (frames.size() > 10)
    {
        // 円の練習場: 中心に照明塔
        Vec3 center{(minX + maxX) * 0.5f, 0, (minZ + maxZ) * 0.5f};
        addFloodTower(mesh, center, 22.0f, lamps);
    }

    // 倉庫: コースの外側（左）に、海沿い以外で間を空けて並べる
    float nextWarehouse{40.0f};
    for (const Frame& frame : frames)
    {
        if (frame.s < nextWarehouse || frame.quayside || frame.alley)
        {
            continue;
        }
        nextWarehouse = frame.s + 75.0f;
        Vec3 outward{frame.right * -1.0f};
        Vec3 base{side(frame, -(frame.wall + CONTAINER_DEPTH + 6.0f), 0)};
        base.y = 0;
        addWarehouse(mesh, base, frame.forward, outward, random);
    }

    addStartGate(mesh, frames.front());
}

}  // namespace adapters::gl
