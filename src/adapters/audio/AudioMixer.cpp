#include "adapters/audio/AudioMixer.hpp"

#include <cmath>
#include <utility>

namespace adapters::audio
{

namespace
{

constexpr float SMOOTH_SEC = 0.04f;  // 音量と速度が目標へ近づく速さ（秒）
constexpr float MASTER_GAIN = 0.9f;

/// 線形補間でサンプルを読む。ループ音は端で先頭へ戻る
float sampleAt(const Sound& sound, double position, bool loop)
{
    const std::size_t count{sound.samples.size()};
    std::size_t i0{static_cast<std::size_t>(position)};
    float t{static_cast<float>(position - double(i0))};
    std::size_t i1{i0 + 1};
    if (loop)
    {
        i0 %= count;
        i1 %= count;
    }
    else if (i1 >= count)
    {
        return i0 < count ? sound.samples[i0] * (1.0f - t) : 0.0f;
    }
    return sound.samples[i0] + (sound.samples[i1] - sound.samples[i0]) * t;
}

}  // namespace

AudioMixer::AudioMixer() = default;

void AudioMixer::setSound(Loop loop, Sound sound)
{
    m_loopSounds[std::size_t(loop)] = std::move(sound);
}

void AudioMixer::setSound(Effect effect, Sound sound)
{
    m_effectSounds[std::size_t(effect)] = std::move(sound);
}

void AudioMixer::setLoop(Loop loop, float gain, float rate)
{
    m_loops[std::size_t(loop)].targetGain.store(gain, std::memory_order_relaxed);
    m_loops[std::size_t(loop)].targetRate.store(rate, std::memory_order_relaxed);
}

void AudioMixer::play(Effect effect, float gain, float rate)
{
    std::size_t head{m_queueHead.load(std::memory_order_relaxed)};
    std::size_t next{(head + 1) % QUEUE_SIZE};
    if (next == m_queueTail.load(std::memory_order_acquire))
    {
        return;  // 待ち行列がいっぱい。鳴らせなくても困らない
    }
    m_queue[head] = {int(effect), gain, rate};
    m_queueHead.store(next, std::memory_order_release);
}

void AudioMixer::startRequested()
{
    std::size_t tail{m_queueTail.load(std::memory_order_relaxed)};
    while (tail != m_queueHead.load(std::memory_order_acquire))
    {
        const Request& request{m_queue[tail]};
        // 空いている声、無ければいちばん長く鳴っている声を使う
        Voice* target{&m_voices[0]};
        for (Voice& voice : m_voices)
        {
            if (voice.effect < 0)
            {
                target = &voice;
                break;
            }
            if (voice.position > target->position)
            {
                target = &voice;
            }
        }
        target->effect = request.effect;
        target->gain = request.gain;
        target->rate = request.rate;
        target->position = 0;
        tail = (tail + 1) % QUEUE_SIZE;
    }
    m_queueTail.store(tail, std::memory_order_release);
}

void AudioMixer::render(float* stereo, int frames, int outputRate)
{
    startRequested();
    const float smooth{1.0f - std::exp(-1.0f / (SMOOTH_SEC * float(outputRate)))};
    for (int frame{}; frame < frames; ++frame)
    {
        float mix{};
        for (std::size_t i{}; i < m_loops.size(); ++i)
        {
            LoopState& state{m_loops[i]};
            const Sound& sound{m_loopSounds[i]};
            state.gain += (state.targetGain.load(std::memory_order_relaxed) - state.gain) * smooth;
            state.rate += (state.targetRate.load(std::memory_order_relaxed) - state.rate) * smooth;
            if (sound.samples.empty())
            {
                continue;
            }
            if (state.gain > 1e-4f)
            {
                mix += sampleAt(sound, state.position, true) * state.gain;
            }
            // 無音の間も読む位置は進める。鳴り始めがいつも同じ所にならないように
            state.position += double(state.rate) * double(sound.sampleRate) / double(outputRate);
            double length{double(sound.samples.size())};
            if (state.position >= length)
            {
                state.position -= length * std::floor(state.position / length);
            }
        }
        for (Voice& voice : m_voices)
        {
            if (voice.effect < 0)
            {
                continue;
            }
            const Sound& sound{m_effectSounds[std::size_t(voice.effect)]};
            if (voice.position >= double(sound.samples.size()))
            {
                voice.effect = -1;
                continue;
            }
            mix += sampleAt(sound, voice.position, false) * voice.gain;
            voice.position += double(voice.rate) * double(sound.sampleRate) / double(outputRate);
        }
        // 重なって大きくなりすぎても割れないよう、なめらかに頭を抑える
        float out{std::tanh(mix * MASTER_GAIN)};
        stereo[frame * 2] = out;
        stereo[frame * 2 + 1] = out;
    }
}

}  // namespace adapters::audio
