#pragma once

#include "adapters/gl/CpuMesh.hpp"
#include "adapters/gl/Renderer.hpp"
#include "domain/Track.hpp"

#include <cstddef>
#include <vector>

namespace adapters::gl
{

/**
 * @brief 道沿いに立てた街灯 1 本
 * @details ドリフトで灯す演出に使う。
 */
struct LampFixture
{
    float s{};            // 周回位置（m）
    domain::Vec3 head;    // 灯りの位置
    std::size_t light{};  // 点の光の並びの中での番号
};

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
 * @param fixtures 道沿いの街灯を受け取る。灯りは消えた色で作る
 */
void buildPortScenery(const domain::Track& track, CpuMesh& mesh, std::vector<PointLight>& lamps,
                      std::vector<LampFixture>& fixtures);

}  // namespace adapters::gl
