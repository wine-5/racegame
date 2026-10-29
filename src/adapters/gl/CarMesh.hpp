#pragma once

#include "adapters/gl/CpuMesh.hpp"
#include "domain/Car.hpp"

namespace adapters::gl
{

// 車の見た目。物理には関係しない。

/**
 * @brief 車体のメッシュを作る
 * @param mesh 作り先
 */
void buildCarBody(CpuMesh& mesh);

/**
 * @brief タイヤ 1 本のメッシュを作る
 * @param mesh 作り先
 */
void buildCarWheel(CpuMesh& mesh);

/**
 * @brief 車の影のメッシュを作る
 * @param mesh 作り先
 */
void buildCarShadow(CpuMesh& mesh);

/**
 * @brief 車体を描くためのモデル行列を作る
 * @details pitch と roll は路面の傾きから描画側で求める。
 * @param car 車の状態
 * @param pitch 前後の傾き（ラジアン）
 * @param roll 左右の傾き（ラジアン）
 * @return モデル行列
 */
domain::Mat4 carModelMatrix(const domain::Car& car, float pitch, float roll);

}  // namespace adapters::gl
