#pragma once

#include "domain/Car.hpp"

#include <SDL.h>

namespace adapters::sdl
{

/**
 * @brief 1 フレーム分の入力
 * @details 運転の入力と、押した瞬間だけ立つ操作。
 */
struct InputFrame
{
    domain::DriveInput drive;
    bool quit{};
    bool reset{};
    bool pause{};
    bool camera{};
    bool help{};
};

/**
 * @brief キーボード・ゲームパッドを読んで InputFrame にする
 * @details ブラウザではパッドを Gamepad API から読む。
 */
class SdlInput
{
public:
    /// つながっているゲームパッドを開く。ブラウザでは何もしない
    void open();

    /// 開いているゲームパッドを閉じる
    void close();

    /**
     * @brief 今フレームの入力を読む
     * @details SDL のイベントを全部取り出す。パッドの抜き差しもここで扱う。
     * @return 今フレームの入力
     */
    InputFrame poll();

private:
    SDL_GameController* m_pad{};
};

}  // namespace adapters::sdl
