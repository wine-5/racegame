#include "adapters/sdl/SdlApp.hpp"

#include <SDL_image.h>
#include <SDL_mixer.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <utility>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

namespace adapters::sdl
{

namespace
{

constexpr double MAX_FRAME_SEC = 0.25;
constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 720;
// ネイティブでフレームを回すときの最短間隔（ミリ秒）
constexpr double NATIVE_FRAME_MS = 14.0;

// 画面を保存するときに、この時間（フレーム）からドリフトのボタンを押し続ける
constexpr int CAPTURE_DRIFT_FROM = 150;

}  // namespace

SdlApp::SdlApp(application::GameSession& session, std::vector<Stage> stages, int firstStage)
    : m_session(session), m_stages(std::move(stages)), m_stageIndex(firstStage)
{
}

SdlApp::~SdlApp()
{
    m_input.close();
    // 音の差し込み口を外してから、音声を閉じる
    m_audio.close();
    if (m_audioOk)
    {
        Mix_CloseAudio();
        Mix_Quit();
    }
    if (m_imageOk)
    {
        IMG_Quit();
    }
    if (m_gl)
    {
        SDL_GL_DeleteContext(m_gl);
    }
    if (m_window)
    {
        SDL_DestroyWindow(m_window);
    }
    if (m_sdlOk)
    {
        SDL_Quit();
    }
}

bool SdlApp::init()
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0)
    {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
    m_sdlOk = true;
    m_imageOk = (IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) != 0;
    if (!m_imageOk)
    {
        SDL_Log("IMG_Init failed: %s", IMG_GetError());
    }
    // 音の遅れを短くするため、バッファは小さめ（1024 フレーム ≒ 23 ms）
    m_audioOk = Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 1024) == 0;
    if (!m_audioOk)
    {
        SDL_Log("Mix_OpenAudio failed: %s", Mix_GetError());
    }
    else if (!m_audio.open())
    {
        SDL_Log("some sounds could not be loaded");
    }
    if (!initWindow())
    {
        return false;
    }
    m_input.open();
    m_prevCounter = SDL_GetPerformanceCounter();
    return true;
}

bool SdlApp::initWindow()
{
#ifdef __EMSCRIPTEN__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    m_window = SDL_CreateWindow("Drift", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT,
                                SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);
    if (!m_window)
    {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }
    m_gl = SDL_GL_CreateContext(m_window);
    if (!m_gl)
    {
        SDL_Log("SDL_GL_CreateContext failed: %s", SDL_GetError());
        return false;
    }
    SDL_GL_SetSwapInterval(1);
    if (!m_view.init())
    {
        SDL_Log("GL init failed");
        return false;
    }
    return loadStage(m_stageIndex);
}

bool SdlApp::loadStage(int index)
{
    if (index < 0 || index >= static_cast<int>(m_stages.size()))
    {
        return false;
    }
    const Stage& stage{m_stages[std::size_t(index)]};
    if (!stage.source || !m_session.start(*stage.source))
    {
        SDL_Log("stage load failed: %s", stage.name.c_str());
        return false;
    }
    m_stageIndex = index;
    m_view.loadStage(m_session.track(), stage.scenery);
    m_driftSound.reset();
    SDL_Log("stage: %s", stage.name.c_str());
    return true;
}

void SdlApp::run()
{
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(&SdlApp::frameThunk, this, 0, 1);
#else
    while (m_running)
    {
        Uint64 frameStart{SDL_GetPerformanceCounter()};
        frame();
        double frameMs{double(SDL_GetPerformanceCounter() - frameStart) / double(SDL_GetPerformanceFrequency()) *
                       1000.0};
        if (frameMs < NATIVE_FRAME_MS)
        {
            SDL_Delay(static_cast<Uint32>(NATIVE_FRAME_MS - frameMs));
        }
    }
#endif
}

void SdlApp::showTitle()
{
    m_screen = Screen::Title;
    if (m_session.paused())
    {
        m_session.togglePause();
    }
    loadStage(m_stageIndex);
}

void SdlApp::startGame()
{
    m_screen = Screen::Playing;
    loadStage(m_stageIndex);
}

void SdlApp::fitCanvas()
{
#ifdef __EMSCRIPTEN__
    // ページの大きさ（CSS）に、キャンバスの画素数を合わせる。高精細な画面では画素を増やすが、重くならないよう 2 倍まで
    double cssW{};
    double cssH{};
    emscripten_get_element_css_size("#canvas", &cssW, &cssH);
    double ratio{std::min(emscripten_get_device_pixel_ratio(), 2.0)};
    int width{static_cast<int>(cssW * ratio)};
    int height{static_cast<int>(cssH * ratio)};
    int currentW{};
    int currentH{};
    emscripten_get_canvas_element_size("#canvas", &currentW, &currentH);
    if (width > 0 && height > 0 && (width != currentW || height != currentH))
    {
        emscripten_set_canvas_element_size("#canvas", width, height);
    }
#endif
}

void SdlApp::toggleFullscreen()
{
#ifndef __EMSCRIPTEN__
    // ブラウザでは shell.html が全画面を切り替える
    bool full{(SDL_GetWindowFlags(m_window) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0};
    SDL_SetWindowFullscreen(m_window, full ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
#endif
}

void SdlApp::frameThunk(void* self)
{
    static_cast<SdlApp*>(self)->frame();
}

void SdlApp::frame()
{
    InputFrame input{m_input.poll()};
    if (input.quit)
    {
        m_running = false;
    }
    const bool playing{m_screen == Screen::Playing};
    if (input.back)
    {
        if (playing)
        {
            showTitle();
        }
        else
        {
#ifndef __EMSCRIPTEN__
            // タイトルで ESC を押すと終わる。ブラウザではタブを閉じればよいので何もしない
            m_running = false;
#endif
        }
    }
    // タイトルでボタンを押した瞬間に、選んだステージを最初から始める
    bool pressed{input.drift && !m_driftWasHeld};
    m_driftWasHeld = input.drift;
    if (!playing && pressed)
    {
        startGame();
    }
    if (playing && input.pause)
    {
        m_session.togglePause();
    }
    if (input.camera)
    {
        m_view.cycleCamera();
    }
    if (input.fullscreen)
    {
        toggleFullscreen();
    }
    if (input.help)
    {
        m_view.toggleHelp();
    }
    if (input.stage >= 0 && input.stage != m_stageIndex)
    {
        loadStage(input.stage);
    }
    if (playing && input.reset)
    {
        m_session.reset();
        m_view.snapCamera();
        m_driftSound.reset();
    }

    Uint64 now{SDL_GetPerformanceCounter()};
    double elapsed{double(now - m_prevCounter) / double(SDL_GetPerformanceFrequency())};
    m_prevCounter = now;
    bool capturing{!m_capture.path.empty()};
    if (capturing)
    {
        elapsed = 1.0 / 60.0;
        // 自動運転は常に効いている。--drift ならボタンを押し、--autopilot なら押さない
        input.drift = m_capture.autoDrive && m_frameCount >= CAPTURE_DRIFT_FROM;
    }
    if (elapsed > MAX_FRAME_SEC)
    {
        elapsed = MAX_FRAME_SEC;
    }
    if (elapsed < 0)
    {
        elapsed = 0;
    }

    // タイトルの後ろでは、デモとしてずっとドリフトさせて見せる
    m_session.advance(elapsed, m_screen == Screen::Playing ? input.drift : true);
    m_driftSound.update(m_session.car(), m_session.drift(), m_session.lastInput().throttle, float(elapsed),
                        m_session.paused());

    fitCanvas();
    int drawableW{};
    int drawableH{};
    SDL_GL_GetDrawableSize(m_window, &drawableW, &drawableH);
#ifdef __EMSCRIPTEN__
    // fitCanvas で変えたキャンバスの大きさを SDL は知らないので、キャンバスから直接読む
    emscripten_get_canvas_element_size("#canvas", &drawableW, &drawableH);
#endif
    if (m_screen == Screen::Title)
    {
        gl::TitleScreen title;
        for (const Stage& stage : m_stages)
        {
            title.stages.push_back(stage.name);
        }
        title.selected = m_stageIndex;
        m_view.render(m_session, float(elapsed), drawableW, drawableH, &title);
    }
    else
    {
        m_view.render(m_session, float(elapsed), drawableW, drawableH);
    }
    if (capturing && m_frameCount >= m_capture.frame)
    {
        saveCapture();
        m_running = false;
    }
    SDL_GL_SwapWindow(m_window);
    ++m_frameCount;
}

void SdlApp::saveCapture()
{
    std::vector<std::uint8_t> rgba;
    int width{};
    int height{};
    m_view.readPixels(rgba, width, height);
    SDL_Surface* surface{SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ABGR8888)};
    if (!surface)
    {
        SDL_Log("SDL_CreateRGBSurfaceWithFormat failed: %s", SDL_GetError());
        return;
    }
    // OpenGL は下の行から並ぶので、上下を入れ替えて書き込む
    for (int row{}; row < height; ++row)
    {
        const std::uint8_t* src{rgba.data() + std::size_t(height - 1 - row) * std::size_t(width) * 4};
        std::uint8_t* dst{static_cast<std::uint8_t*>(surface->pixels) + std::size_t(row) * std::size_t(surface->pitch)};
        std::copy(src, src + std::size_t(width) * 4, dst);
    }
    if (IMG_SavePNG(surface, m_capture.path.c_str()) != 0)
    {
        SDL_Log("IMG_SavePNG failed: %s", IMG_GetError());
    }
    else
    {
        SDL_Log("saved %s", m_capture.path.c_str());
    }
    SDL_FreeSurface(surface);
}

}  // namespace adapters::sdl
