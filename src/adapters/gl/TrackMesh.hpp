#pragma once

#include "adapters/gl/CpuMesh.hpp"
#include "domain/Track.hpp"

// コースの見た目（路面、芝、縁石、壁、ピット、スタートライン）を作る。
void buildTrackMesh(const Track& track, CpuMesh& mesh);
