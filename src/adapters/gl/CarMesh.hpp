#pragma once

#include "adapters/gl/CpuMesh.hpp"
#include "domain/Car.hpp"

namespace adapters::gl
{

// 車の見た目。物理には関係しない。
void buildCarBody(CpuMesh& mesh);
void buildCarWheel(CpuMesh& mesh);
void buildCarShadow(CpuMesh& mesh);
// 車体の描画用の姿勢。pitch と roll は路面の傾きから描画側で求める。
domain::Mat4 carModelMatrix(const domain::Car& car, float pitch, float roll);

}  // namespace adapters::gl
