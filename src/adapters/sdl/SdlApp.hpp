#pragma once

#include "adapters/audio/AudioMixer.hpp"
#include "adapters/audio/DriftSound.hpp"
#include "adapters/gl/GameView.hpp"
#include "adapters/gl/Scenery.hpp"
#include "adapters/sdl/SdlAudio.hpp"
#include "adapters/sdl/SdlInput.hpp"
#include "application/GameSession.hpp"
#include "application/ports/ITrackSource.hpp"

#include <SDL.h>

#include <string>
#include <vector>

namespace adapters::sdl
{

/**
 * @brief 遊べるステージ 1 つ分
 */
struct Stage
{
    std::string name;
    application::ports::ITrackSource* source{};  // コースの読み込み元。SdlApp より長く生きていること
    gl::Scenery scenery = gl::Scenery::Circuit;
};

/**
 * @brief 画面を画像に保存する開発用の設定
 * @details 演出の見た目を、画面を見られない環境でも確かめるために使う。
 */
struct CaptureOptions
{
    std::string path;  // 保存先の PNG。空なら保存しない
    int frame{180};    // このフレームを描いたら保存して終わる
    bool autoDrive{};  // 途中からドリフトのボタンを押し続ける
    bool autopilot{};  // ボタンを押さずに自動運転で走る。コースの奥の景色を確かめるのに使う
    bool title{};  // タイトル画面を保存する
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
     * @brief 動かすゲームとステージを指定して作る
     * @param session 動かすゲーム。SdlApp より長く生きていること
     * @param stages 選べるステージ。数字キーの 1 から順に割り当てる
     * @param firstStage 最初に遊ぶステージの番号
     */
    SdlApp(application::GameSession& session, std::vector<Stage> stages, int firstStage);

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
    void setCapture(const CaptureOptions& options)
    {
        m_capture = options;
        // 画面を保存するときは、タイトルを飛ばしてすぐ遊ぶ画面にする（--title のときだけタイトル）
        m_screen = options.path.empty() || options.title ? Screen::Title : Screen::Playing;
    }

private:
    /// emscripten_set_main_loop_arg に渡すための入口
    static void frameThunk(void* self);

    /// 1 フレーム分の処理
    void frame();

    /// ウィンドウと GL コンテキストを作り、GameView を初期化する
    bool initWindow();

    /// 画面を PNG に保存する
    void saveCapture();

    /// ステージを読み込み、車をスタート位置に置く
    bool loadStage(int index);

    /// タイトル画面へ戻る。後ろではデモの走りを見せる
    void showTitle();

    /// 選んでいるステージを最初から遊び始める
    void startGame();

    /// ブラウザでは、キャンバスの画素数をページの大きさに合わせる
    void fitCanvas();

    /// 全画面とウィンドウを切り替える（ネイティブのみ）
    void toggleFullscreen();

    application::GameSession& m_session;
    std::vector<Stage> m_stages;
    int m_stageIndex{};
    SdlInput m_input;
    audio::AudioMixer m_mixer;
    audio::DriftSound m_driftSound{m_mixer};
    SdlAudio m_audio{m_mixer};
    gl::GameView m_view;
    SDL_Window* m_window{};
    SDL_GLContext m_gl{};
    bool m_sdlOk{};
    bool m_imageOk{};
    bool m_audioOk{};
    bool m_running{true};
    Uint64 m_prevCounter{};
    /// 今出している画面
    enum class Screen
    {
        Title,
        Playing
    };

    Screen m_screen = Screen::Title;
    bool m_driftWasHeld{};
    CaptureOptions m_capture;
    int m_frameCount{};
};

}  // namespace adapters::sdl
