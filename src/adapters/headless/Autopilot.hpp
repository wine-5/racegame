#pragma once

#include "domain/Car.hpp"
#include "domain/Track.hpp"

namespace adapters::headless
{

/**
 * @brief コースに沿って走る簡単な自動運転（動かす側のアダプター）
 * @details 70m 先の中心線へ向けてステアを切り、曲がりがきついと減速する。
 *          人の代わりに入力を作るので、画面なしのテストや、演出の画面の確認に使える。
 * @param car 車の状態
 * @param track 走っているコース
 * @return 今のステップの入力
 */
domain::DriveInput autopilot(const domain::Car& car, const domain::Track& track);

}  // namespace adapters::headless
