#pragma once

#include "domain/Track.hpp"

namespace adapters::headless
{

/**
 * @brief 画面なしで 50 秒走らせて、コースと物理が壊れていないか調べる
 * @details 簡単な自動運転でコースに沿って走る。結果を標準出力に書く。
 * @param track 走るコース
 * @return 0 なら成功。1: 値が非有限、2: コースの全長がおかしい、3: 進んだ距離が短い、4: コースから外れた
 */
int runSelfTest(const domain::Track& track);

}  // namespace adapters::headless
