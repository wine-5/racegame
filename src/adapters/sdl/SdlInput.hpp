#pragma once

#include "domain/Car.hpp"

#include <SDL.h>

namespace adapters::sdl
{

// 1 フレーム分の入力。運転の入力と、押した瞬間だけ立つ操作。
struct InputFrame
{
    domain::DriveInput drive;
    bool quit{};
    bool reset{};
    bool pause{};
    bool camera{};
    bool help{};
};

// キーボード・ゲームパッド（ブラウザでは Gamepad API）を読んで InputFrame にする。
class SdlInput
{
public:
    void open();
    void close();
    // SDL のイベントを全部取り出して、今フレームの入力を返す。
    InputFrame poll();

private:
    SDL_GameController* m_pad{};
};

}  // namespace adapters::sdl
