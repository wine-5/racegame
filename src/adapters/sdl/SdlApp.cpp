#include "adapters/sdl/SdlApp.hpp"

#include <SDL_image.h>
#include <SDL_mixer.h>

#include <cstdio>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
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

}  // namespace

SdlApp::SdlApp(application::GameSession& session) : m_session(session)
{
}

SdlApp::~SdlApp()
{
    m_input.close();
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
    m_audioOk = Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) == 0;
    if (!m_audioOk)
    {
        SDL_Log("Mix_OpenAudio failed: %s", Mix_GetError());
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
    m_window = SDL_CreateWindow("Suzuka Circuit", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH,
                                WINDOW_HEIGHT, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);
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
    if (!m_view.init(m_session.track()))
    {
        SDL_Log("GL init failed");
        return false;
    }
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
    if (input.pause)
    {
        m_session.togglePause();
    }
    if (input.camera)
    {
        m_view.cycleCamera();
    }
    if (input.help)
    {
        m_view.toggleHelp();
    }
    if (input.reset)
    {
        m_session.reset();
        m_view.snapCamera();
    }

    Uint64 now{SDL_GetPerformanceCounter()};
    double elapsed{double(now - m_prevCounter) / double(SDL_GetPerformanceFrequency())};
    m_prevCounter = now;
    if (elapsed > MAX_FRAME_SEC)
    {
        elapsed = MAX_FRAME_SEC;
    }
    if (elapsed < 0)
    {
        elapsed = 0;
    }

    m_session.advance(elapsed, input.drive);

    int drawableW{};
    int drawableH{};
    SDL_GL_GetDrawableSize(m_window, &drawableW, &drawableH);
    m_view.render(m_session, float(elapsed), drawableW, drawableH);
    SDL_GL_SwapWindow(m_window);
}

}  // namespace adapters::sdl
