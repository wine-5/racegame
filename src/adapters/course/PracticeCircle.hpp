#pragma once

#include "application/ports/ITrackSource.hpp"

namespace adapters::course
{

/**
 * @brief 円を回り続ける練習場（定常円旋回）
 * @details 幅の広い円で、角度を保ったまま回り続ける練習と、車の挙動の調整に使う。
 */
class PracticeCircle : public application::ports::ITrackSource
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
