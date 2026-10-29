#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <vector>

namespace adapters::audio
{

/**
 * @brief 1 つの音のデータ（モノラル）
 */
struct Sound
{
    std::vector<float> samples;  // -1..1
    int sampleRate{44100};
};

/// 鳴り続ける音
enum class Loop
{
    Engine,
    Squeal,
    Count
};

/// 1 回だけ鳴る音
enum class Effect
{
    DriftStart,
    Combo,
    Bank,
    Crash,
    LampOn,   // 街灯が灯った
    ConeHit,  // パイロンを弾いた
    DrumHit,  // ドラム缶を弾いた
    PortLit,  // 1 周で街灯を全部灯した
    Count
};

/**
 * @brief ループ音と効果音を混ぜて出力する
 * @details ループ音は再生速度を変えて音の高さを変える。音量と速度はなめらかに目標へ近づくので、
 *          急に変えてもプツプツしない。
 *          setLoop と play はゲームのスレッド、render は音声のスレッドから呼んでよい（ロックなしで受け渡す）。
 *          SDL に依存しないので、画面や音声機器が無くてもテストできる。
 */
class AudioMixer
{
public:
    AudioMixer();

    /**
     * @brief ループ音のデータを入れる
     * @details 音を鳴らし始める前に呼ぶ。
     * @param loop どのループ音か
     * @param sound 音のデータ
     */
    void setSound(Loop loop, Sound sound);

    /**
     * @brief 効果音のデータを入れる
     * @details 音を鳴らし始める前に呼ぶ。
     * @param effect どの効果音か
     * @param sound 音のデータ
     */
    void setSound(Effect effect, Sound sound);

    /**
     * @brief ループ音の音量と音の高さを決める
     * @param loop どのループ音か
     * @param gain 音量（0 で無音）
     * @param rate 再生速度（1 で元の高さ、2 で 1 オクターブ上）
     */
    void setLoop(Loop loop, float gain, float rate);

    /**
     * @brief 効果音を 1 回鳴らす
     * @param effect どの効果音か
     * @param gain 音量
     * @param rate 再生速度（音の高さ）
     */
    void play(Effect effect, float gain, float rate);

    /**
     * @brief 音を混ぜて書き出す
     * @param stereo 書き出し先。左右交互に frames × 2 個
     * @param frames 書き出すフレーム数
     * @param outputRate 出力のサンプリング周波数
     */
    void render(float* stereo, int frames, int outputRate);

private:
    struct LoopState
    {
        std::atomic<float> targetGain{0.0f};
        std::atomic<float> targetRate{1.0f};
        float gain{};
        float rate{1.0f};
        double position{};
    };

    struct Voice
    {
        int effect{-1};  // -1 なら鳴っていない
        float gain{};
        float rate{1.0f};
        double position{};
    };

    struct Request
    {
        int effect{};
        float gain{};
        float rate{};
    };

    static constexpr int MAX_VOICES = 12;
    static constexpr std::size_t QUEUE_SIZE = 64;

    /// 鳴らしてほしい効果音を取り出して、空いている声に割り当てる
    void startRequested();

    std::array<Sound, std::size_t(Loop::Count)> m_loopSounds;
    std::array<Sound, std::size_t(Effect::Count)> m_effectSounds;
    std::array<LoopState, std::size_t(Loop::Count)> m_loops;
    std::array<Voice, MAX_VOICES> m_voices;
    // ゲームのスレッドが書き、音声のスレッドが読む 1 対 1 の待ち行列
    std::array<Request, QUEUE_SIZE> m_queue;
    std::atomic<std::size_t> m_queueHead{0};
    std::atomic<std::size_t> m_queueTail{0};
};

}  // namespace adapters::audio
