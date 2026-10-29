#include "adapters/gl/CarMesh.hpp"

#include <cmath>

namespace adapters::gl
{

using domain::basis;
using domain::Car;
using domain::cross;
using domain::Mat4;
using domain::normalize;
using domain::Vec3;

void buildCarBody(CpuMesh& mesh)
{
    const Vec3 red{0.72f, 0.08f, 0.09f};
    const Vec3 dark{0.06f, 0.06f, 0.07f};
    const Vec3 carbon{0.12f, 0.12f, 0.13f};
    mesh.addBox({-0.88f, 0.32f, -2.05f}, {0.88f, 0.62f, 2.05f}, red);
    mesh.addBox({-0.72f, 0.62f, -0.55f}, {0.72f, 1.05f, 1.05f}, red);
    mesh.addBox({-0.62f, 0.78f, -0.35f}, {0.62f, 1.02f, 0.9f}, dark);
    mesh.addBox({-0.7f, 0.32f, 1.7f}, {0.7f, 0.5f, 2.25f}, red);
    mesh.addBox({-0.95f, 0.26f, 1.95f}, {0.95f, 0.36f, 2.35f}, carbon);
    mesh.addBox({-0.86f, 0.85f, -2.2f}, {0.86f, 1.0f, -1.85f}, carbon);
    mesh.addBox({-0.15f, 0.95f, -2.05f}, {0.15f, 1.15f, -1.9f}, carbon);
}

void buildCarWheel(CpuMesh& mesh)
{
    const int seg{14};
    const float radius{0.34f};
    const float halfW{0.15f};
    const Vec3 tire{0.07f, 0.07f, 0.08f};
    const Vec3 rim{0.72f, 0.74f, 0.76f};
    for (int i = 0; i < seg; ++i)
    {
        float a0{float(i) / float(seg) * 6.2831853f};
        float a1{float(i + 1) / float(seg) * 6.2831853f};
        float c0{std::cos(a0)};
        float s0{std::sin(a0)};
        float c1{std::cos(a1)};
        float s1{std::sin(a1)};
        Vec3 normal{normalize(Vec3{0, c0 + c1, s0 + s1})};
        Vec3 a{-halfW, c0 * radius, s0 * radius};
        Vec3 b{halfW, c0 * radius, s0 * radius};
        Vec3 c{halfW, c1 * radius, s1 * radius};
        Vec3 d{-halfW, c1 * radius, s1 * radius};
        mesh.addQuad(a, b, c, d, normal, tire);
        for (int side = -1; side <= 1; side += 2)
        {
            float x{float(side) * halfW};
            Vec3 nrm{float(side), 0, 0};
            Vec3 center{x, 0, 0};
            Vec3 p0{x, c0 * radius * 0.62f, s0 * radius * 0.62f};
            Vec3 p1{x, c1 * radius * 0.62f, s1 * radius * 0.62f};
            Vec3 o0{x, c0 * radius, s0 * radius};
            Vec3 o1{x, c1 * radius, s1 * radius};
            mesh.addQuad(o0, o1, p1, p0, nrm, tire);
            mesh.addTriangle(center, p0, p1, nrm, rim);
        }
    }
}

void buildCarShadow(CpuMesh& mesh)
{
    const Vec3 ink{0.02f, 0.02f, 0.025f};
    mesh.addQuad({-1.05f, 0, -2.1f}, {1.05f, 0, -2.1f}, {1.05f, 0, 2.15f}, {-1.05f, 0, 2.15f}, {0, 1, 0}, ink);
}

Mat4 carModelMatrix(const Car& car, float pitch, float roll)
{
    Vec3 forward = normalize(Vec3{
        std::sin(car.yaw) * std::cos(pitch),
        std::sin(pitch),
        std::cos(car.yaw) * std::cos(pitch),
    });
    Vec3 right0{std::cos(car.yaw), 0, -std::sin(car.yaw)};
    Vec3 up0{normalize(cross(forward, right0))};
    Vec3 right{normalize(right0 * std::cos(roll) + up0 * std::sin(roll))};
    Vec3 up{normalize(cross(forward, right))};
    return basis({car.x, car.y, car.z}, right, up, forward);
}

}  // namespace adapters::gl
