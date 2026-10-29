#pragma once

#include "domain/Geom.hpp"

#include <string>
#include <vector>

namespace domain
{

struct TrackPose
{
    bool found{};
    float s{};
    float lateral{};  // 右が正
    float centerY{};
    float cross{};  // 右方向 1m あたりの高さ
    float yaw{};
    float rightX{1};
    float rightZ{};
    float xzDist{};
    int corner{};
    int nameId{-1};
};

struct TrackSample
{
    float x{};
    float z{};
    float y{};
    float cross{};
    float s{};
    int corner{};
    int nameId{-1};
    bool bridge{};
};

// 鈴鹿サーキット。中心線は OSM、高さは国土地理院の標高。
struct Track
{
    float originLat{};
    float originLon{};
    float halfWidth{6.5f};
    float curbWidth{0.8f};
    float runoff{8.0f};
    float length{};
    float spawnX{};
    float spawnY{};
    float spawnZ{};
    float spawnYaw{};
    std::vector<TrackSample> samples;
    std::vector<std::string> names;
    std::vector<Vec3> pit;  // ピットレーンの中心線。走行には使わない

    // samples を埋めたあとに呼ぶ。周回距離 s と全長を計算する。点が足りなければ false。
    bool finalize();
    // preferS が 0 以上のときは、その周回位置に近い路面を優先する。立体交差で上下を取り違えない。
    TrackPose query(float x, float z, float yHint, float preferS = -1.0f) const;
    float wallLateral() const { return halfWidth + curbWidth + runoff; }
    const char* nameOf(int nameId) const;
};

}  // namespace domain
