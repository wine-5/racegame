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

constexpr float STICK_DEADZONE = 0.15f;  // スティックの遊び。これより小さい傾きは 0 にする

/// スティックの傾き（-1..1）から遊びを取り除く
float applyDeadzone(float value)
{
    if (std::fabs(value) < STICK_DEADZONE)
    {
        return 0.0f;
    }
    return (value - std::copysign(STICK_DEADZONE, value)) / (1.0f - STICK_DEADZONE);
}

/// パッドの入力を、キーボードの入力に重ねる。強い方を使う
void mergePad(InputFrame& frame, float steer, float throttle, float brake, bool drift)
{
    steer = applyDeadzone(steer);
    if (std::fabs(steer) > std::fabs(frame.steer))
    {
        frame.steer = steer;
    }
    frame.throttle = std::fmax(frame.throttle, throttle);
    frame.brake = std::fmax(frame.brake, brake);
    frame.drift = frame.drift || drift;
}

#ifdef __EMSCRIPTEN__
/**
 * ブラウザのゲームパッドを Gamepad API から読む。SDL はブラウザのパッドを
 * ゲームコントローラと認識しないことが多いため。標準の並びで、左スティック（軸 0）でステア、
 * RT（7）でアクセル、LT（6）でブレーキ、A（0）でドリフト。
 */
PadPulse readBrowserPad(InputFrame& frame)
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
        float stick{pad.numAxes > 0 ? static_cast<float>(pad.axis[0]) : 0.0f};
        mergePad(frame, stick, analog(7), analog(6), held(0));
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

/// 運転の操作を読む。キーボードは W / S / A / D と矢印キー、Space、マウスの左ボタン
void readDriving(SDL_GameController* pad, PadPulse& pulse, InputFrame& frame)
{
    const Uint8* keys{SDL_GetKeyboardState(nullptr)};
    auto down = [&](SDL_Scancode a, SDL_Scancode b) { return keys[a] != 0 || keys[b] != 0; };
    frame.throttle = down(SDL_SCANCODE_W, SDL_SCANCODE_UP) ? 1.0f : 0.0f;
    frame.brake = down(SDL_SCANCODE_S, SDL_SCANCODE_DOWN) ? 1.0f : 0.0f;
    frame.steer = (down(SDL_SCANCODE_D, SDL_SCANCODE_RIGHT) ? 1.0f : 0.0f) -
                  (down(SDL_SCANCODE_A, SDL_SCANCODE_LEFT) ? 1.0f : 0.0f);
    frame.drift = keys[SDL_SCANCODE_SPACE] != 0;
    frame.drift = frame.drift || (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK) != 0;
#ifndef __EMSCRIPTEN__
    if (pad)
    {
        auto axis = [&](SDL_GameControllerAxis which)
        { return static_cast<float>(SDL_GameControllerGetAxis(pad, which)) / 32767.0f; };
        mergePad(frame, axis(SDL_CONTROLLER_AXIS_LEFTX), axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT),
                 axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT), SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A) != 0);
    }
    (void)pulse;
#else
    (void)pad;
    PadPulse browser{readBrowserPad(frame)};
    pulse.camera = pulse.camera || browser.camera;
    pulse.reset = pulse.reset || browser.reset;
    pulse.pause = pulse.pause || browser.pause;
#endif
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
                frame.back = true;
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
    readDriving(m_pad, padPulse, frame);
    frame.reset = frame.reset || padPulse.reset;
    frame.pause = frame.pause || padPulse.pause;
    frame.camera = frame.camera || padPulse.camera;
    return frame;
}

}  // namespace adapters::sdl
