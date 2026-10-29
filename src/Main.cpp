#include "adapters/file/TrackFile.hpp"
#include "adapters/gl/CarMesh.hpp"
#include "adapters/gl/ChaseCamera.hpp"
#include "adapters/gl/Hud.hpp"
#include "adapters/gl/Renderer.hpp"
#include "adapters/gl/TrackMesh.hpp"
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
using adapters::gl::buildCarBody;
using adapters::gl::buildCarShadow;
using adapters::gl::buildCarWheel;
using adapters::gl::buildTrackMesh;
using adapters::gl::carModelMatrix;
using adapters::gl::ChaseCamera;
using adapters::gl::CpuMesh;
using adapters::gl::drawHud;
using adapters::gl::GpuMesh;
using adapters::gl::Renderer;
using adapters::sdl::InputFrame;
using adapters::sdl::SdlInput;
using application::GameSession;
using domain::Car;
using domain::clampf;
using domain::DriveInput;
using domain::lookAt;
using domain::Mat4;
using domain::mul;
using domain::perspective;
using domain::rotationX;
using domain::rotationY;
using domain::Track;
using domain::TrackPose;
using domain::TrackSample;
using domain::translation;
using domain::Vec3;
using domain::wrapPi;

constexpr double MAX_FRAME_SEC = 0.25;

struct App
{
    SDL_Window* window{};
    SDL_GLContext gl{};
    SdlInput input;
    Renderer renderer;
    GameSession session;
    GpuMesh course;
    GpuMesh body;
    GpuMesh wheel;
    GpuMesh shadow;
    ChaseCamera camera;
    bool running{true};
    bool showHelp{true};
    Uint64 prevCounter{};
};

void resetRace(App& app)
{
    app.session.reset();
    app.camera.snap();
}

float carPitch(const App& app)
{
    Vec3 forward{std::sin(app.session.car().yaw), 0, std::cos(app.session.car().yaw)};
    float preferS = app.session.car().ground.found ? app.session.car().ground.s : -1.0f;
    TrackPose ahead = app.session.track().query(app.session.car().x + forward.x * 2.4f,
                                                app.session.car().z + forward.z * 2.4f, app.session.car().y, preferS);
    TrackPose behind = app.session.track().query(app.session.car().x - forward.x * 2.4f,
                                                 app.session.car().z - forward.z * 2.4f, app.session.car().y, preferS);
    if (!ahead.found || !behind.found)
    {
        return 0;
    }
    return std::atan2(ahead.centerY - behind.centerY, 4.8f);
}

void frame(void* userdata)
{
    App& app = *static_cast<App*>(userdata);
    InputFrame input = app.input.poll();
    if (input.quit)
        app.running = false;
    if (input.pause)
        app.session.togglePause();
    if (input.camera)
        app.camera.cycleMode();
    if (input.help)
        app.showHelp = !app.showHelp;
    if (input.reset)
    {
        resetRace(app);
    }

    Uint64 now = SDL_GetPerformanceCounter();
    double elapsed = double(now - app.prevCounter) / double(SDL_GetPerformanceFrequency());
    app.prevCounter = now;
    if (elapsed > MAX_FRAME_SEC)
        elapsed = MAX_FRAME_SEC;
    if (elapsed < 0)
        elapsed = 0;

    app.session.advance(elapsed, input.drive);
    app.camera.update(app.session.car(), app.session.track(), float(elapsed));

    int drawableW = 0;
    int drawableH = 0;
    SDL_GL_GetDrawableSize(app.window, &drawableW, &drawableH);
    app.renderer.resize(drawableW, drawableH);
    app.renderer.clearFrame();
    app.renderer.drawSky();

    float aspect = float(drawableW) / float(drawableH > 0 ? drawableH : 1);
    Mat4 proj = perspective(60.0f * 3.14159265f / 180.0f, aspect, 0.15f, 4000.0f);
    Mat4 view = lookAt(app.camera.eye(), app.camera.look(), {0, 1, 0});
    app.renderer.beginWorld(mul(proj, view), app.camera.eye());

    Mat4 identity;
    app.renderer.draw(app.course, identity);
    Mat4 shadow = mul(translation(app.session.car().x, app.session.car().y + 0.08f, app.session.car().z),
                      rotationY(app.session.car().yaw));
    app.renderer.draw(app.shadow, shadow);

    float pitch = carPitch(app);
    float roll = std::atan(app.session.car().ground.cross);
    Mat4 model = carModelMatrix(app.session.car(), pitch, roll);
    app.renderer.draw(app.body, model);
    const float wheelX = 0.82f;
    const float wheelZ[2] = {1.42f, -1.38f};
    const float steerForWheel[2] = {app.session.car().steerAngle, 0};
    for (int i = 0; i < 2; ++i)
    {
        for (int side = -1; side <= 1; side += 2)
        {
            Mat4 place = mul(translation(float(side) * wheelX, 0.34f, wheelZ[i]),
                             mul(rotationY(steerForWheel[i]), rotationX(app.session.car().wheelSpin)));
            app.renderer.draw(app.wheel, mul(model, place));
        }
    }
    drawHud(app.renderer, app.session, app.camera, app.showHelp);
    SDL_GL_SwapWindow(app.window);

#ifndef __EMSCRIPTEN__
    if (!app.running)
    {
        // ネイティブの while が抜ける。
    }
#endif
}

int selfTest(const Track& track)
{
    Car car;
    car.reset(track);
    float travelled = 0;
    float prevS = -1;
    float maxLat = 0;
    for (int step = 0; step < 60 * 50; ++step)
    {
        TrackPose pose = track.query(car.x, car.z, car.y, car.ground.found ? car.ground.s : -1.0f);
        float lookS = pose.s + 70.0f;
        if (lookS >= track.length)
        {
            lookS -= track.length;
        }
        int nearest = 0;
        float nearestGap = 1.0e9f;
        for (int i = 0; i < static_cast<int>(track.samples.size()); i += 2)
        {
            float gap = std::fabs(track.samples[i].s - lookS);
            if (gap > track.length * 0.5f)
            {
                gap = track.length - gap;
            }
            if (gap < nearestGap)
            {
                nearestGap = gap;
                nearest = i;
            }
        }
        int next = (nearest + 1) % static_cast<int>(track.samples.size());
        float tx = track.samples[next].x - track.samples[nearest].x;
        float tz = track.samples[next].z - track.samples[nearest].z;
        float targetYaw = std::atan2(tx, tz);
        float errNow = wrapPi(pose.yaw - car.yaw);
        float errAhead = wrapPi(targetYaw - car.yaw);
        float bend = std::fabs(wrapPi(targetYaw - pose.yaw));
        DriveInput input;
        input.steer = clampf(errNow * 2.6f + errAhead * 0.5f, -1.0f, 1.0f);
        input.steer = clampf(input.steer - pose.lateral * 0.12f, -1.0f, 1.0f);
        input.throttle = bend < 0.15f ? 1.0f : 0.45f;
        input.brake = 0;
        if (bend > 0.28f && car.speed > 22.0f)
        {
            input.throttle = 0;
            input.brake = 1.0f;
        }
        else if (bend > 0.12f && car.speed > 34.0f)
        {
            input.throttle = 0;
            input.brake = 0.55f;
        }
        if (car.speed < 10.0f)
        {
            input.brake = 0;
            input.throttle = std::max(input.throttle, 0.5f);
        }
        car.update(1.0f / 60.0f, input, track);
        pose = track.query(car.x, car.z, car.y, car.ground.found ? car.ground.s : -1.0f);
        if (prevS >= 0)
        {
            float ds = pose.s - prevS;
            if (ds < -track.length * 0.5f)
                ds += track.length;
            if (ds > track.length * 0.5f)
                ds -= track.length;
            if (ds > 0)
                travelled += ds;
        }
        prevS = pose.s;
        maxLat = std::max(maxLat, pose.xzDist);
        if (!std::isfinite(car.x) || !std::isfinite(car.speed) || !std::isfinite(car.yaw))
        {
            std::printf("self-test non-finite\n");
            return 1;
        }
    }
    std::printf("self-test length %.0f travelled %.0f maxLat %.1f speed %.1f km/h\n", track.length, travelled, maxLat,
                car.speed * 3.6f);
    if (track.length < 5200.0f || track.length > 6500.0f)
        return 2;
    if (travelled < 1200.0f)
        return 3;
    if (maxLat > 30.0f)
        return 4;
    return 0;
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
    if (!app.renderer.init())
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
        return selfTest(session.track());
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
    CpuMesh course;
    buildTrackMesh(app.session.track(), course);
    app.course = app.renderer.upload(course);
    CpuMesh body;
    CpuMesh wheel;
    CpuMesh shadow;
    buildCarBody(body);
    buildCarWheel(wheel);
    buildCarShadow(shadow);
    app.body = app.renderer.upload(body);
    app.wheel = app.renderer.upload(wheel);
    app.shadow = app.renderer.upload(shadow);
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
