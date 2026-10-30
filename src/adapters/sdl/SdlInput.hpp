#pragma once

#include <SDL.h>

namespace adapters::sdl
{

/**
 * @brief 1 フレーム分の入力
 * @details 運転の操作（押している間ずっと効く）と、押した瞬間だけ立つ操作。
 */
struct InputFrame
{
    float throttle{};  // アクセル（0..1）
    float brake{};     // ブレーキ（0..1）
    float steer{};     // ステア（-1..1）。左が負
    bool drift{};      // ドリフトのボタンを押しているか
    bool quit{};       // ウィンドウを閉じた
    bool back{};       // ESC。遊んでいるときはタイトルへ戻る
    bool reset{};
    bool pause{};
    bool camera{};
    bool help{};
    bool fullscreen{};  // 全画面の切り替え（F）
    int stage{-1};      // 数字キーで選んだステージの番号（0 から）。押していなければ -1
    int stageStep{};    // 十字キーの左右で、ステージを前後へ選ぶ（-1 / 0 / 1）
};

/**
 * @brief キーボード・マウス・ゲームパッドを読んで InputFrame にする
 * @details 運転は W / S / A / D（矢印キーも可）、ドリフトは Space かマウスの左ボタン（タッチ）。
 *          パッドは左スティック（十字キー）でステア、RT でアクセル、LT でブレーキ、A か RB でドリフト、
 *          B でタイトルへ戻る。タイトルでは十字キーの左右でステージを選ぶ。
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

    /**
     * @brief つながっているパッドを振動させる
     * @details ブラウザでは Gamepad API の振動（対応しているブラウザとパッドだけ）を使う。
     * @param strong 低い周波数の大きなモーターの強さ（0..1）
     * @param weak 高い周波数の小さなモーターの強さ（0..1）
     * @param durationMs 振動させる時間（ミリ秒）
     */
    void rumble(float strong, float weak, int durationMs);

private:
    SDL_GameController* m_pad{};
};

}  // namespace adapters::sdl
