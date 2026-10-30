#pragma once

#include "adapters/sdl/SdlInput.hpp"
#include "application/GameSession.hpp"

namespace adapters::sdl
{

/**
 * @brief ゲームの様子に合わせてパッドを振動させる
 * @details ドリフト中は滑り角に合わせて細かく震わせる。倍率アップ・街灯の点灯・小物を弾いた瞬間・
 *          壁への衝突・全部灯したときは、強さと長さを変えて短く揺らす。
 */
class PadRumble
{
public:
    /**
     * @brief 振動させる先を指定して作る
     * @param input 振動させるパッドを持つ入力。PadRumble より長く生きていること
     */
    explicit PadRumble(SdlInput& input) : m_input(input) {}

    /**
     * @brief 1 フレーム分、振動を決める
     * @param session ゲームの状態
     * @param dt 前のフレームからの経過時間（秒）
     */
    void update(const application::GameSession& session, float dt);

    /// 振動を止めて、前の状態を忘れる。タイトルへ戻ったときやリセットのあとに呼ぶ
    void reset();

private:
    /// 短く揺らす。今の揺れより強ければ上書きする
    void pulse(float strong, float weak, float seconds);

    SdlInput& m_input;
    float m_pulseLeft{};      // 短い揺れの残り時間（秒）
    float m_pulseStrength{};  // 今の短い揺れの強さ
    float m_sinceSend{};      // ドリフト中の揺れを最後に送ってからの時間（秒）
    bool m_wasHitWall{};
    int m_lastMultiplier{1};
    int m_lastIgnitions{};
    int m_lastHits{};
    float m_lastSinceAllLit{1e9f};
};

}  // namespace adapters::sdl
