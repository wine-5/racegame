#pragma once

#include "adapters/gl/CpuMesh.hpp"
#include "adapters/gl/Renderer.hpp"
#include "domain/Track.hpp"

#include <vector>

namespace adapters::gl
{

/**
 * @brief 夜の港の空と光の当たり方
 * @return 月明かりと、暗い紺色の霧
 */
Lighting portLighting();

/// 夜の港の路面の色。タイヤ痕を路面になじませるのに使う
domain::Vec3 portRoadColor();

/**
 * @brief 夜の港の景色を作る
 * @details 路面・黄色の縁取り・コンテナの壁・街灯・ネオン・クレーン・倉庫・海と対岸の灯りを作る。
 *          QUAYSIDE という区間があれば、その左側を海にする。
 * @param track コース
 * @param mesh 作り先
 * @param lamps 街灯などの点の光を受け取る
 */
void buildPortScenery(const domain::Track& track, CpuMesh& mesh, std::vector<PointLight>& lamps);

}  // namespace adapters::gl
