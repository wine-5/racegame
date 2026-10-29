#include "adapters/file/TrackFile.hpp"
#include "adapters/gl/GameView.hpp"
#include "adapters/headless/SelfTest.hpp"
#include "adapters/sdl/SdlInput.hpp"
#include "application/GameSession.hpp"

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>

#include <cstring>
#endif

namespace
{

using adapters::file::TrackFile;
using adapters::gl::GameView;
using adapters::headless::runSelfTest;
using adapters::sdl::InputFrame;
using adapters::sdl::SdlInput;
using application::GameSession;

constexpr double MAX_FRAME_SEC = 0.25;

struct App
{
    SDL_Window* window{};
    SDL_GLContext gl{};
    SdlInput input;
    GameView view;
    GameSession session;
    bool running{true};
    Uint64 prevCounter{};
};

void frame(void* userdata)
{
    App& app = *static_cast<App*>(userdata);
    InputFrame input = app.input.poll();
    if (input.quit)
        app.running = false;
    if (input.pause)
        app.session.togglePause();
    if (input.camera)
        app.view.cycleCamera();
    if (input.help)
        app.view.toggleHelp();
    if (input.reset)
    {
        app.session.reset();
        app.view.snapCamera();
    }

    Uint64 now = SDL_GetPerformanceCounter();
    double elapsed = double(now - app.prevCounter) / double(SDL_GetPerformanceFrequency());
    app.prevCounter = now;
    if (elapsed > MAX_FRAME_SEC)
        elapsed = MAX_FRAME_SEC;
    if (elapsed < 0)
        elapsed = 0;

    app.session.advance(elapsed, input.drive);

    int drawableW = 0;
    int drawableH = 0;
    SDL_GL_GetDrawableSize(app.window, &drawableW, &drawableH);
    app.view.render(app.session, float(elapsed), drawableW, drawableH);
    SDL_GL_SwapWindow(app.window);

#ifndef __EMSCRIPTEN__
    if (!app.running)
    {
        // ネイティブの while が抜ける。
    }
#endif
}

bool initGl(App& app)
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
    app.window = SDL_CreateWindow("Suzuka Circuit", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720,
                                  SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);
    if (!app.window)
    {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }
    app.gl = SDL_GL_CreateContext(app.window);
    if (!app.gl)
    {
        SDL_Log("SDL_GL_CreateContext failed: %s", SDL_GetError());
        return false;
    }
    SDL_GL_SetSwapInterval(1);
    if (!app.view.init(app.session.track()))
    {
        SDL_Log("GL init failed");
        return false;
    }
    return true;
}

}  // namespace

int main(int argc, char** argv)
{
    bool test = argc > 1 && std::strcmp(argv[1], "--self-test") == 0;
    GameSession session;
    TrackFile trackFile(TrackFile::findDefaultPath());
    if (!session.start(trackFile))
    {
        std::fprintf(stderr, "コースを読めません: %s\n", trackFile.path().c_str());
        return 1;
    }
    if (test)
    {
        return runSelfTest(session.track());
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0)
    {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    bool imageOk = (IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) != 0;
    if (!imageOk)
    {
        SDL_Log("IMG_Init failed: %s", IMG_GetError());
    }
    bool audioOk = Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) == 0;
    if (!audioOk)
    {
        SDL_Log("Mix_OpenAudio failed: %s", Mix_GetError());
    }

    App app;
    app.session = std::move(session);
    if (!initGl(app))
    {
        SDL_Quit();
        return 1;
    }
    app.input.open();
    app.prevCounter = SDL_GetPerformanceCounter();

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(frame, &app, 0, 1);
#else
    while (app.running)
    {
        Uint64 frameStart = SDL_GetPerformanceCounter();
        frame(&app);
        double frameMs =
            double(SDL_GetPerformanceCounter() - frameStart) / double(SDL_GetPerformanceFrequency()) * 1000.0;
        if (frameMs < 14.0)
        {
            SDL_Delay(static_cast<Uint32>(14.0 - frameMs));
        }
    }
#endif

    app.input.close();
    if (audioOk)
    {
        Mix_CloseAudio();
        Mix_Quit();
    }
    if (imageOk)
        IMG_Quit();
    SDL_GL_DeleteContext(app.gl);
    SDL_DestroyWindow(app.window);
    SDL_Quit();
    return 0;
}
