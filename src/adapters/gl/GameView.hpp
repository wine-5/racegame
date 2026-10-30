#pragma once

#include "adapters/gl/Bursts.hpp"
#include "adapters/gl/ChaseCamera.hpp"
#include "adapters/gl/Hud.hpp"
#include "adapters/gl/LightTrail.hpp"
#include "adapters/gl/PortScenery.hpp"
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
     * @brief 景色に立てた街灯の周回位置
     * @details loadStage のあと GameSession::setLampSpots に渡す。街灯の無いステージでは空。
     * @return 周回位置（m）の並び
     */
    const std::vector<float>& lampSpots() const { return m_lampSpots; }

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
        m_trail.clear();
        m_bursts.clear();
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

    /// 弾き飛ばせる小物を描く
    void drawProps(const domain::Props& props);

    /**
     * @brief カメラに近い街灯と車のライトを選んで Renderer に渡す
     * @details ドリフトで灯した街灯は、倍率の色で明るくする。
     * @param car 車の状態
     * @param lamps 街灯の状態
     */
    void chooseLights(const domain::Car& car, const domain::StreetLamps& lamps);

    /**
     * @brief 街灯が灯った瞬間と、小物を弾いた瞬間に光の粒を散らす
     * @param session ゲームの状態
     */
    void react(const application::GameSession& session);

    /**
     * @brief 街灯の灯りを光る板として追加する
     * @param lamps 街灯の状態
     * @param out 追加先
     */
    void collectLampHalos(const domain::StreetLamps& lamps, std::vector<Billboard>& out) const;

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
    std::vector<PointLight> m_lamps;      // 景色に立つ街灯などの光
    std::vector<PointLight> m_lampWork;   // 灯した色を反映した光。並べ替えて使う
    std::vector<PointLight> m_lights;     // このフレームで当てる光
    std::vector<LampFixture> m_fixtures;  // 道沿いの街灯
    std::vector<float> m_lampSpots;       // 道沿いの街灯の周回位置
    std::vector<int> m_shownTier;  // 街灯ごとに、光の粒を散らしたときの倍率。0 は消えている
    int m_lastHits{};
    float m_lastSinceAllLit{1e9f};
    GpuMesh m_course;
    GpuMesh m_body;
    GpuMesh m_wheel;
    GpuMesh m_shadow;
    GpuMesh m_cone;
    GpuMesh m_drum;
    ChaseCamera m_camera;
    TireSmoke m_smoke;
    Sparks m_sparks;
    LightTrail m_trail;
    Bursts m_bursts;
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
