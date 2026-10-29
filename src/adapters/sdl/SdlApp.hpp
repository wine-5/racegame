#pragma once

#include "adapters/gl/GameView.hpp"
#include "adapters/sdl/SdlInput.hpp"
#include "application/GameSession.hpp"

#include <SDL.h>

namespace adapters::sdl
{

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

private:
    /// emscripten_set_main_loop_arg に渡すための入口
    static void frameThunk(void* self);

    /// 1 フレーム分の処理
    void frame();

    /// ウィンドウと GL コンテキストを作り、GameView を初期化する
    bool initWindow();

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
};

}  // namespace adapters::sdl
