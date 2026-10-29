#pragma once

#include <SDL.h>

namespace adapters::sdl
{

/**
 * @brief 1 フレーム分の入力
 * @details ドリフトのボタン（押している間ずっと立つ）と、押した瞬間だけ立つ操作。
 */
struct InputFrame
{
    bool drift{};  // ドリフトのボタンを押しているか
    bool quit{};
    bool reset{};
    bool pause{};
    bool camera{};
    bool help{};
    bool fullscreen{};  // 全画面の切り替え（F）
    int stage{-1};      // 数字キーで選んだステージの番号（0 から）。押していなければ -1
};

/**
 * @brief キーボード・マウス・ゲームパッドを読んで InputFrame にする
 * @details ドリフトのボタンは Space、マウスの左ボタン（タッチ）、パッドの A か RT。
 *          ブラウザではパッドを Gamepad API から読む。
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
