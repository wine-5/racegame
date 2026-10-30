#pragma once

#include "adapters/gl/ChaseCamera.hpp"
#include "adapters/gl/Renderer.hpp"
#include "application/GameSession.hpp"

#include <string>
#include <vector>

namespace adapters::gl
{

/**
 * @brief HUD を描く
 * @details 速度、周回とタイム、コーナー名、カメラのモード、操作説明、ポーズ表示、ミニマップ。
 * @param renderer 描画先
 * @param session 表示するゲームの状態
 * @param camera 今のカメラ（モード名を表示する）
 * @param showHelp 操作説明を表示するか
 * @param showCredit 地図データの出典を表示するか（鈴鹿のとき）
 */
void drawHud(Renderer& renderer, const application::GameSession& session, const ChaseCamera& camera, bool showHelp,
             bool showCredit);

/**
 * @brief タイトル画面に出すもの
 */
struct TitleScreen
{
    std::vector<std::string> stages;  // 選べるステージの名前
    int selected{};                   // 選んでいるステージ
};

/**
 * @brief タイトル画面を描く
 * @details 後ろではデモの走りを見せたまま、題名・始め方・ステージの一覧を重ねる。
 * @param renderer 描画先
 * @param title 出すもの
 * @param time タイトル画面を出してからの時間（秒）。点滅に使う
 */
void drawTitle(Renderer& renderer, const TitleScreen& title, float time);

}  // namespace adapters::gl
