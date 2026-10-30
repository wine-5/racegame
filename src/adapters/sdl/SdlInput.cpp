#include "adapters/sdl/SdlInput.hpp"

#include <cmath>
#include <cstring>

#ifdef __EMSCRIPTEN__
#include <emscripten/em_js.h>
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
    bool back{};      // B: タイトルへ戻る
    int stageStep{};  // 十字キーの左右: タイトルでステージを選ぶ
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
// ブラウザのパッドを振動させる。振動に対応していないブラウザやパッドでは何もしない
// clang-format off
EM_JS(void, browserRumble, (double strong, double weak, int durationMs), {
    const pads = navigator.getGamepads ? navigator.getGamepads() : [];
    for (const pad of pads) {
        if (pad && pad.vibrationActuator) {
            pad.vibrationActuator.playEffect('dual-rumble', {
                duration: durationMs, strongMagnitude: strong, weakMagnitude: weak
            }).catch(function() {});
        }
    }
});
// clang-format on

/**
 * ブラウザのゲームパッドを Gamepad API から読む。SDL はブラウザのパッドを
 * ゲームコントローラと認識しないことが多いため。標準の並びで、左スティック（軸 0）でステア、
 * RT（7）でアクセル、LT（6）でブレーキ、A（0）でドリフト。
 */
PadPulse readBrowserPad(InputFrame& frame)
{
    // 押した瞬間だけ立てたいボタンの番号（標準の並び）: Y, Back, Start, B, 十字キーの左, 右
    constexpr int PULSE_BUTTONS[]{3, 8, 9, 1, 14, 15};
    constexpr int PULSE_COUNT{static_cast<int>(sizeof(PULSE_BUTTONS) / sizeof(PULSE_BUTTONS[0]))};
    static bool previous[PULSE_COUNT]{};
    bool now[PULSE_COUNT]{};
    PadPulse pulse;
    if (emscripten_sample_gamepad_data() != EMSCRIPTEN_RESULT_SUCCESS)
    {
        for (bool& held : previous)
        {
            held = false;
        }
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
        // 十字キーの左右でもステアできる
        float dpad{(held(15) ? 1.0f : 0.0f) - (held(14) ? 1.0f : 0.0f)};
        mergePad(frame, std::fabs(dpad) > 0.0f ? dpad : stick, analog(7), analog(6), held(0) || held(5));
        for (int b{}; b < PULSE_COUNT; ++b)
        {
            now[b] = now[b] || held(PULSE_BUTTONS[b]);
        }
    }
    bool pressed[PULSE_COUNT]{};
    for (int b{}; b < PULSE_COUNT; ++b)
    {
        pressed[b] = now[b] && !previous[b];
        previous[b] = now[b];
    }
    pulse.camera = pressed[0];
    pulse.reset = pressed[1];
    pulse.pause = pressed[2];
    pulse.back = pressed[3];
    pulse.stageStep = (pressed[5] ? 1 : 0) - (pressed[4] ? 1 : 0);
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
        auto button = [&](SDL_GameControllerButton which) { return SDL_GameControllerGetButton(pad, which) != 0; };
        // 十字キーの左右でもステアできる
        float dpad{(button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT) ? 1.0f : 0.0f) -
                   (button(SDL_CONTROLLER_BUTTON_DPAD_LEFT) ? 1.0f : 0.0f)};
        mergePad(frame, std::fabs(dpad) > 0.0f ? dpad : axis(SDL_CONTROLLER_AXIS_LEFTX),
                 axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT), axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT),
                 button(SDL_CONTROLLER_BUTTON_A) || button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER));
    }
    (void)pulse;
#else
    (void)pad;
    PadPulse browser{readBrowserPad(frame)};
    pulse.camera = pulse.camera || browser.camera;
    pulse.reset = pulse.reset || browser.reset;
    pulse.pause = pulse.pause || browser.pause;
    pulse.back = pulse.back || browser.back;
    pulse.stageStep += browser.stageStep;
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
            else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_B)
                frame.back = true;
            else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT)
                --frame.stageStep;
            else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT)
                ++frame.stageStep;
        }
    }
    PadPulse padPulse;
    readDriving(m_pad, padPulse, frame);
    frame.reset = frame.reset || padPulse.reset;
    frame.pause = frame.pause || padPulse.pause;
    frame.camera = frame.camera || padPulse.camera;
    frame.back = frame.back || padPulse.back;
    frame.stageStep += padPulse.stageStep;
    return frame;
}

void SdlInput::rumble(float strong, float weak, int durationMs)
{
    strong = std::fmin(std::fmax(strong, 0.0f), 1.0f);
    weak = std::fmin(std::fmax(weak, 0.0f), 1.0f);
#ifdef __EMSCRIPTEN__
    browserRumble(strong, weak, durationMs);
#else
    if (m_pad)
    {
        SDL_GameControllerRumble(m_pad, static_cast<Uint16>(strong * 65535.0f), static_cast<Uint16>(weak * 65535.0f),
                                 static_cast<Uint32>(durationMs));
    }
#endif
}

}  // namespace adapters::sdl
