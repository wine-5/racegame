#pragma once

#include "adapters/gl/ChaseCamera.hpp"
#include "adapters/gl/Renderer.hpp"
#include "application/GameSession.hpp"

namespace adapters::gl
{

/**
 * @brief HUD を描く
 * @details 速度、周回とタイム、コーナー名、カメラのモード、操作説明、ポーズ表示、ミニマップ。
 * @param renderer 描画先
 * @param session 表示するゲームの状態
 * @param camera 今のカメラ（モード名を表示する）
 * @param showHelp 操作説明を表示するか
 */
void drawHud(Renderer& renderer, const application::GameSession& session, const ChaseCamera& camera, bool showHelp);

}  // namespace adapters::gl
