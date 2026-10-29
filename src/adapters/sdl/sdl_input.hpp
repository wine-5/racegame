#pragma once

#include "domain/car.hpp"

#include <SDL.h>

// 1 フレーム分の入力。運転の入力と、押した瞬間だけ立つ操作。
struct InputFrame {
    DriveInput drive;
    bool quit = false;
    bool reset = false;
    bool pause = false;
    bool camera = false;
    bool help = false;
};

// キーボード・ゲームパッド（ブラウザでは Gamepad API）を読んで InputFrame にする。
class SdlInput {
public:
    void open();
    void close();
    // SDL のイベントを全部取り出して、今フレームの入力を返す。
    InputFrame poll();

private:
    SDL_GameController* pad_ = nullptr;
};
