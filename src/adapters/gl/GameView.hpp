#pragma once

#include "adapters/gl/ChaseCamera.hpp"
#include "adapters/gl/Hud.hpp"
#include "adapters/gl/PostProcess.hpp"
#include "adapters/gl/Renderer.hpp"
#include "adapters/gl/Scenery.hpp"
#include "adapters/gl/SkidMarks.hpp"
#include "adapters/gl/Sparks.hpp"
#include "adapters/gl/TireSmoke.hpp"
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
     * @brief シェーダーを作り、車のメッシュを GPU に送る
     * @details GL コンテキストを作ったあとに呼ぶ。そのあと loadStage でコースを読み込む。
     * @return 失敗したら false
     */
    bool init();

    /**
     * @brief ステージの景色を作り直す
     * @details 煙やタイヤ痕も消し、カメラを車の後ろへ戻す。
     * @param track 描くコース
     * @param scenery 景色の種類
     */
    void loadStage(const domain::Track& track, Scenery scenery);

    /**
     * @brief 1 フレーム描く
     * @param session 描くゲームの状態
     * @param dt 前のフレームからの経過時間（秒）。カメラの補間に使う
     * @param width 描画先の幅（ピクセル）
     * @param height 描画先の高さ（ピクセル）
     * @param title タイトル画面を出すなら、その中身。遊んでいるときは nullptr
     */
    void render(const application::GameSession& session, float dt, int width, int height,
                const TitleScreen* title = nullptr);

    /// カメラを補間せずに車の後ろへ戻し、煙などの演出を消す。リセットのあとに呼ぶ
    void snapCamera()
    {
        m_camera.snap();
        m_smoke.clear();
        m_skidMarks.clear();
        m_sparks.clear();
        m_skidMarksDirty = true;
    }

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

    /// カメラに近い街灯と車のライトを選んで Renderer に渡す
    void chooseLights(const domain::Car& car);

    /**
     * @brief 今のゲームの様子から、画面全体の効果の強さを決める
     * @details 速いほど端がぶれ、深く滑るほど色がずれて鮮やかになる。倍率アップ・得点の確定・
     *          クラッシュの瞬間には画面を光らせる。
     */
    PostEffects feel(const application::GameSession& session, float dt);

    Renderer m_renderer;
    PostProcess m_post;
    float m_flash{};
    domain::Vec3 m_flashColor{1.0f, 1.0f, 1.0f};
    int m_lastMultiplier{1};
    float m_lastSinceResult{1e9f};
    Scenery m_scenery = Scenery::Circuit;
    std::vector<PointLight> m_lamps;   // 景色に立つ街灯などの光
    std::vector<PointLight> m_lights;  // このフレームで当てる光
    GpuMesh m_course;
    GpuMesh m_body;
    GpuMesh m_wheel;
    GpuMesh m_shadow;
    ChaseCamera m_camera;
    TireSmoke m_smoke;
    Sparks m_sparks;
    std::vector<Billboard> m_glows;  // 光る板（火花・光の尾）
    SkidMarks m_skidMarks;
    CpuMesh m_skidMesh;
    GpuMesh m_skidGpu;
    bool m_skidMarksDirty{};
    std::vector<Billboard> m_billboards;
    bool m_showHelp{true};
    float m_time{};  // 描き始めてからの時間（秒）
};

}  // namespace adapters::gl
