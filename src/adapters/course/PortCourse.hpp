#pragma once

#include "application/ports/ITrackSource.hpp"

namespace adapters::course
{

/**
 * @brief 夜の港を一周するドリフト用のコース
 * @details 海沿いの直線 → クレーン広場の大きなヘアピン → コンテナの間の S 字 → 倉庫の角 →
 *          長い一定半径のカーブ、の順に並ぶ。広い所で角度を保ち、狭い所で切り返しを楽しむ。
 */
class PortCourse : public application::ports::ITrackSource
{
public:
    /**
     * @brief コースを組み立てる
     * @param track 書き込み先
     * @return 常に true
     */
    bool load(domain::Track& track) override;
};

}  // namespace adapters::course
