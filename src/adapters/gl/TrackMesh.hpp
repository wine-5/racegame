#pragma once

#include "adapters/gl/CpuMesh.hpp"
#include "domain/Track.hpp"

namespace adapters::gl
{

/**
 * @brief コースの見た目を作る
 * @details 路面、芝、縁石、壁、ピット、スタートライン。
 * @param track コース
 * @param mesh 作り先
 */
void buildTrackMesh(const domain::Track& track, CpuMesh& mesh);

}  // namespace adapters::gl
