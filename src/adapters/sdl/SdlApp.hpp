#pragma once

#include "adapters/gl/GameView.hpp"
#include "adapters/sdl/SdlInput.hpp"
#include "application/GameSession.hpp"

#include <SDL.h>

#include <string>

namespace adapters::sdl
{

/**
 * @brief 画面を画像に保存する開発用の設定
 * @details 演出の見た目を、画面を見られない環境でも確かめるために使う。
 */
struct CaptureOptions
{
    std::string path;  // 保存先の PNG。空なら保存しない
    int frame{180};    // このフレームを描いたら保存して終わる
    bool autoDrive{};  // 決まった操作で加速してドリフトに入る
};

/**
 * @brief SDL のウィンドウとメインループ（動かす側のアダプター）
 * @details 毎フレーム、入力を読み → GameSession を進め → GameView に描かせる。
 *          ネイティブでは while で回し、ブラウザではブラウザのフレームに合わせて呼ばれる。
 */
class SdlApp
{
public:
    /**
     * @brief 動かすゲームを指定して作る
     * @param session 動かすゲーム。SdlApp より長く生きていること
     */
    explicit SdlApp(application::GameSession& session);

    /// 初期化したものを逆順で片付ける
    ~SdlApp();

    SdlApp(const SdlApp&) = delete;
    SdlApp& operator=(const SdlApp&) = delete;

    /**
     * @brief SDL・画像・音声・ウィンドウ・GL を初期化する
     * @return ウィンドウか GL の初期化に失敗したら false。画像と音声は失敗しても続ける
     */
    bool init();

    /**
     * @brief メインループを回す
     * @details ネイティブでは終了するまで戻らない。ブラウザでは戻らずにブラウザへ制御を渡す。
     */
    void run();

    /**
     * @brief 画面を画像に保存して終わるようにする（開発用）
     * @details 保存するときは、経過時間を 1/60 秒に固定して毎回同じ絵になるようにする。
     * @param options 保存の設定
     */
    void setCapture(const CaptureOptions& options) { m_capture = options; }

private:
    /// emscripten_set_main_loop_arg に渡すための入口
    static void frameThunk(void* self);

    /// 1 フレーム分の処理
    void frame();

    /// ウィンドウと GL コンテキストを作り、GameView を初期化する
    bool initWindow();

    /// 画面を PNG に保存する
    void saveCapture();

    application::GameSession& m_session;
    SdlInput m_input;
    gl::GameView m_view;
    SDL_Window* m_window{};
    SDL_GLContext m_gl{};
    bool m_sdlOk{};
    bool m_imageOk{};
    bool m_audioOk{};
    bool m_running{true};
    Uint64 m_prevCounter{};
    CaptureOptions m_capture;
    int m_frameCount{};
};

}  // namespace adapters::sdl
