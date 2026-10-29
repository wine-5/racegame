#pragma once

#include "adapters/gl/meshbuild.hpp"
#include "domain/track.hpp"

// コースの見た目（路面、芝、縁石、壁、ピット、スタートライン）を作る。
void buildTrackMesh(const Track& track, CpuMesh& mesh);
