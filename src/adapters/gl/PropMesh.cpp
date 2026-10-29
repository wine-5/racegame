#include "adapters/gl/PropMesh.hpp"

#include "adapters/gl/Renderer.hpp"

#include <cmath>

namespace adapters::gl
{

using domain::Mat4;
using domain::Vec3;

namespace
{

constexpr int SEGMENTS = 14;
constexpr float TWO_PI = 6.2831853f;
const Vec3 CONE_ORANGE{1.0f, 0.38f, 0.06f};
const Vec3 DRUM_RED{0.62f, 0.08f, 0.06f};

/**
 * 輪切りの台形を回して側面を作る。下の半径 r0（高さ y0）から上の半径 r1（高さ y1）まで
 */
void addRing(CpuMesh& mesh, float r0, float y0, float r1, float y1, Vec3 color)
{
    float slope{(r0 - r1) / std::fmax(y1 - y0, 1e-4f)};
    for (int i{}; i < SEGMENTS; ++i)
    {
        float a0{TWO_PI * float(i) / float(SEGMENTS)};
        float a1{TWO_PI * float(i + 1) / float(SEGMENTS)};
        float am{(a0 + a1) * 0.5f};
        Vec3 normal{normalize(Vec3{std::sin(am), slope, std::cos(am)})};
        Vec3 p0{std::sin(a0) * r0, y0, std::cos(a0) * r0};
        Vec3 p1{std::sin(a1) * r0, y0, std::cos(a1) * r0};
        Vec3 p2{std::sin(a1) * r1, y1, std::cos(a1) * r1};
        Vec3 p3{std::sin(a0) * r1, y1, std::cos(a0) * r1};
        mesh.addQuad(p0, p1, p2, p3, normal, color);
    }
}

/// 高さ y の円の蓋
void addDisc(CpuMesh& mesh, float radius, float y, Vec3 color)
{
    Vec3 center{0, y, 0};
    for (int i{}; i < SEGMENTS; ++i)
    {
        float a0{TWO_PI * float(i) / float(SEGMENTS)};
        float a1{TWO_PI * float(i + 1) / float(SEGMENTS)};
        mesh.addTriangle(center, {std::sin(a0) * radius, y, std::cos(a0) * radius},
                         {std::sin(a1) * radius, y, std::cos(a1) * radius}, {0, 1, 0}, color);
    }
}

}  // namespace

void buildConeMesh(CpuMesh& mesh)
{
    mesh = CpuMesh{};
    mesh.addBox({-0.3f, 0.0f, -0.3f}, {0.3f, 0.05f, 0.3f}, Vec3{0.08f, 0.08f, 0.09f});
    // 下から オレンジ → 光る白い帯 → オレンジ
    addRing(mesh, 0.24f, 0.05f, 0.17f, 0.32f, CONE_ORANGE);
    addRing(mesh, 0.17f, 0.32f, 0.13f, 0.46f, glow(Vec3{0.55f, 0.55f, 0.55f}));
    addRing(mesh, 0.13f, 0.46f, 0.03f, 0.75f, CONE_ORANGE);
}

void buildDrumMesh(CpuMesh& mesh)
{
    mesh = CpuMesh{};
    const float radius{0.3f};
    addRing(mesh, radius, 0.0f, radius, 0.3f, DRUM_RED);
    addRing(mesh, radius + 0.005f, 0.3f, radius + 0.005f, 0.42f, glow(Vec3{0.6f, 0.45f, 0.05f}));
    addRing(mesh, radius, 0.42f, radius, 0.88f, DRUM_RED);
    addDisc(mesh, radius, 0.88f, DRUM_RED * 0.7f);
}

Mat4 propModelMatrix(const domain::Prop& prop)
{
    // 倒れた小物は、横倒しの半径の分だけ持ち上げて地面にめり込まないようにする
    float lift{std::sin(prop.tilt) * (prop.kind == domain::PropKind::Cone ? 0.2f : 0.3f)};
    return mul(domain::translation(prop.position.x, prop.position.y + lift, prop.position.z),
               mul(domain::rotationY(prop.yaw), domain::rotationX(prop.tilt)));
}

}  // namespace adapters::gl
