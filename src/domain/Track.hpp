#pragma once

#include "domain/Geom.hpp"

#include <string>
#include <vector>

namespace domain
{

/**
 * @brief コース上のある点についての問い合わせ結果
 */
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

/**
 * @brief コースの中心線の 1 点
 */
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

/**
 * @brief コースのデータと位置の問い合わせ
 * @details 鈴鹿サーキット。中心線は OSM、高さは国土地理院の標高。
 */
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

    /**
     * @brief 周回距離と全長を計算する
     * @details samples を埋めたあとに呼ぶ。
     * @return 点が足りなければ false
     */
    bool finalize();

    /**
     * @brief 指定した位置にいちばん近い路面を探す
     * @details preferS が 0 以上のときは、その周回位置に近い路面を優先する。立体交差で上下を取り違えない。
     * @param x 位置の x
     * @param z 位置の z
     * @param yHint 位置の高さ。立体交差で上下を選ぶのに使う
     * @param preferS 優先する周回位置。負なら使わない
     * @return 見つかった路面の情報。見つからなければ found が false
     */
    TrackPose query(float x, float z, float yHint, float preferS = -1.0f) const;

    /// 中心線から壁までの横方向の距離
    float wallLateral() const { return halfWidth + curbWidth + runoff; }

    /**
     * @brief コーナー名を取得する
     * @param nameId 名前の番号
     * @return コーナー名。番号が範囲外なら空文字列
     */
    const char* nameOf(int nameId) const;
};

}  // namespace domain
