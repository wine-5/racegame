#pragma once

#include "domain/car.hpp"
#include "adapters/gl/meshbuild.hpp"

// 車の見た目。物理には関係しない。
void buildCarBody(CpuMesh& mesh);
void buildCarWheel(CpuMesh& mesh);
void buildCarShadow(CpuMesh& mesh);
// 車体の描画用の姿勢。pitch と roll は路面の傾きから描画側で求める。
Mat4 carModelMatrix(const Car& car, float pitch, float roll);
