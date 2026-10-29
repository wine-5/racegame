#include "adapters/sdl/SdlInput.hpp"

#include <cmath>
#include <cstring>

#ifdef __EMSCRIPTEN__
#include <emscripten/html5.h>
#endif

namespace adapters::sdl
{

namespace
{

/// 押した瞬間だけ立てたいボタン
struct PadPulse
{
    bool reset{};
    bool pause{};
    bool camera{};
};

#ifdef __EMSCRIPTEN__
/**
 * ブラウザのゲームパッドを Gamepad API から読む。SDL はブラウザのパッドを
 * ゲームコントローラと認識しないことが多いため。標準の並びで、A（0）か RT（7）でドリフト。
 */
PadPulse readBrowserPad(bool& drift)
{
    PadPulse pulse;
    static bool prevCamera{false};
    static bool prevReset{false};
    static bool prevPause{false};
    bool camera{false};
    bool reset{false};
    bool pause{false};
    if (emscripten_sample_gamepad_data() != EMSCRIPTEN_RESULT_SUCCESS)
    {
        prevCamera = prevReset = prevPause = false;
        return pulse;
    }
    const int count{emscripten_get_num_gamepads()};
    for (int i = 0; i < count; ++i)
    {
        EmscriptenGamepadEvent pad;
        if (emscripten_get_gamepad_status(i, &pad) != EMSCRIPTEN_RESULT_SUCCESS || !pad.connected)
        {
            continue;
        }
        auto held = [&](int index) { return index < pad.numButtons && pad.digitalButton[index]; };
        auto analog = [&](int index)
        { return index < pad.numButtons ? static_cast<float>(pad.analogButton[index]) : 0.0f; };
        drift = drift || held(0) || analog(7) > 0.3f;
        camera = camera || held(3);
        reset = reset || held(8);
        pause = pause || held(9);
    }
    pulse.camera = camera && !prevCamera;
    pulse.reset = reset && !prevReset;
    pulse.pause = pause && !prevPause;
    prevCamera = camera;
    prevReset = reset;
    prevPause = pause;
    return pulse;
}
#endif

/// ドリフトのボタン: Space、マウスの左ボタン（タッチも含む）、パッドの A か RT
bool readDriftButton(SDL_GameController* pad, PadPulse& pulse)
{
    const Uint8* keys{SDL_GetKeyboardState(nullptr)};
    bool drift{keys[SDL_SCANCODE_SPACE] != 0};
    drift = drift || (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK) != 0;
#ifndef __EMSCRIPTEN__
    if (pad)
    {
        drift = drift || SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A);
        drift = drift || SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 10000;
    }
    (void)pulse;
#else
    (void)pad;
    PadPulse browser{readBrowserPad(drift)};
    pulse.camera = pulse.camera || browser.camera;
    pulse.reset = pulse.reset || browser.reset;
    pulse.pause = pulse.pause || browser.pause;
#endif
    return drift;
}

}  // namespace

void SdlInput::open()
{
#ifdef __EMSCRIPTEN__
    // ブラウザのパッドは Gamepad API 側で読む。SDL で開くと入力が二重になる。
    return;
#else
    if (m_pad)
    {
        return;
    }
    for (int i = 0; i < SDL_NumJoysticks(); ++i)
    {
        if (SDL_IsGameController(i))
        {
            m_pad = SDL_GameControllerOpen(i);
            if (m_pad)
            {
                SDL_Log("controller: %s", SDL_GameControllerName(m_pad));
                break;
            }
        }
    }
#endif
}

void SdlInput::close()
{
    if (m_pad)
    {
        SDL_GameControllerClose(m_pad);
        m_pad = nullptr;
    }
}

InputFrame SdlInput::poll()
{
    InputFrame frame;
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_QUIT)
        {
            frame.quit = true;
        }
        else if (event.type == SDL_KEYDOWN && !event.key.repeat)
        {
            SDL_Scancode code{event.key.keysym.scancode};
            if (code == SDL_SCANCODE_ESCAPE)
                frame.quit = true;
            else if (code == SDL_SCANCODE_R)
                frame.reset = true;
            else if (code == SDL_SCANCODE_C)
                frame.camera = true;
            else if (code == SDL_SCANCODE_P)
                frame.pause = true;
            else if (code == SDL_SCANCODE_H)
                frame.help = true;
            else if (code == SDL_SCANCODE_F)
                frame.fullscreen = true;
            else if (code >= SDL_SCANCODE_1 && code <= SDL_SCANCODE_9)
                frame.stage = code - SDL_SCANCODE_1;
        }
        else if (event.type == SDL_CONTROLLERDEVICEADDED)
        {
            open();
        }
        else if (event.type == SDL_CONTROLLERDEVICEREMOVED && m_pad &&
                 event.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(m_pad)))
        {
            close();
        }
        else if (event.type == SDL_CONTROLLERBUTTONDOWN)
        {
            if (event.cbutton.button == SDL_CONTROLLER_BUTTON_BACK)
                frame.reset = true;
            else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_START)
                frame.pause = true;
            else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_Y)
                frame.camera = true;
        }
    }
    PadPulse padPulse;
    frame.drift = readDriftButton(m_pad, padPulse);
    frame.reset = frame.reset || padPulse.reset;
    frame.pause = frame.pause || padPulse.pause;
    frame.camera = frame.camera || padPulse.camera;
    return frame;
}

}  // namespace adapters::sdl
