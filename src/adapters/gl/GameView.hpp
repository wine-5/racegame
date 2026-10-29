#pragma once

#include "adapters/gl/ChaseCamera.hpp"
#include "adapters/gl/Renderer.hpp"
#include "application/GameSession.hpp"

#include <cstdint>
#include <vector>

namespace adapters::gl
{

/**
 * @brief ゲームの画面を描く（描画アダプター）
 * @details GameSession の状態を読むだけで、書き換えない。コースと車のメッシュ、カメラ、HUD をまとめて持つ。
 */
class GameView
{
public:
    /**
     * @brief シェーダーを作り、コースと車のメッシュを GPU に送る
     * @details GL コンテキストを作ったあとに呼ぶ。
     * @param track 描くコース
     * @return 失敗したら false
     */
    bool init(const domain::Track& track);

    /**
     * @brief 1 フレーム描く
     * @param session 描くゲームの状態
     * @param dt 前のフレームからの経過時間（秒）。カメラの補間に使う
     * @param width 描画先の幅（ピクセル）
     * @param height 描画先の高さ（ピクセル）
     */
    void render(const application::GameSession& session, float dt, int width, int height);

    /// カメラを補間せずに車の後ろへ戻す。リセットのあとに呼ぶ
    void snapCamera() { m_camera.snap(); }

    /// カメラのモードを切り替える
    void cycleCamera() { m_camera.cycleMode(); }

    /// 操作説明の表示を切り替える
    void toggleHelp() { m_showHelp = !m_showHelp; }

    /**
     * @brief 最後に描いた画面を読み出す
     * @param rgba 読み出し先。下の行から順に並ぶ
     * @param width 幅（ピクセル）を受け取る
     * @param height 高さ（ピクセル）を受け取る
     */
    void readPixels(std::vector<std::uint8_t>& rgba, int& width, int& height) const;

private:
    /// 車と影とタイヤを描く
    void drawCar(const domain::Car& car, const domain::Track& track);

    Renderer m_renderer;
    GpuMesh m_course;
    GpuMesh m_body;
    GpuMesh m_wheel;
    GpuMesh m_shadow;
    ChaseCamera m_camera;
    bool m_showHelp{true};
};

}  // namespace adapters::gl
