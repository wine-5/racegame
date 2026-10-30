#include "adapters/sdl/SdlAudio.hpp"

#include <SDL_mixer.h>

#include <algorithm>
#include <fstream>

namespace adapters::sdl
{

using audio::Effect;
using audio::Loop;
using audio::Sound;

namespace
{

constexpr int MAX_FRAMES = 4096;  // 1 回の呼び出しで混ぜる最大のフレーム数

/// 実行場所によって assets の場所が変わるので探す
std::string findSound(const std::string& name)
{
    const char* dirs[]{"assets/sound/", "../assets/sound/", "/assets/sound/"};
    for (const char* dir : dirs)
    {
        std::string path{std::string{dir} + name};
        if (std::ifstream{path})
        {
            return path;
        }
    }
    return std::string{dirs[0]} + name;
}

}  // namespace

SdlAudio::SdlAudio(audio::AudioMixer& mixer) : m_mixer(mixer)
{
}

SdlAudio::~SdlAudio()
{
    close();
}

bool SdlAudio::load(const std::string& name, Sound& sound)
{
    std::string path{findSound(name)};
    SDL_AudioSpec spec;
    Uint8* data{};
    Uint32 length{};
    if (!SDL_LoadWAV(path.c_str(), &spec, &data, &length))
    {
        SDL_Log("SDL_LoadWAV failed: %s (%s)", path.c_str(), SDL_GetError());
        return false;
    }
    // どんな形式でも、32 bit 浮動小数のモノラルにそろえる
    SDL_AudioCVT cvt;
    SDL_BuildAudioCVT(&cvt, spec.format, spec.channels, spec.freq, AUDIO_F32SYS, 1, spec.freq);
    std::vector<Uint8> work(std::size_t(length) * std::size_t(std::max(cvt.len_mult, 1)));
    std::copy(data, data + length, work.begin());
    SDL_FreeWAV(data);
    cvt.buf = work.data();
    cvt.len = int(length);
    if (cvt.needed && SDL_ConvertAudio(&cvt) != 0)
    {
        SDL_Log("SDL_ConvertAudio failed: %s", SDL_GetError());
        return false;
    }
    int bytes{cvt.needed ? cvt.len_cvt : int(length)};
    const float* samples{reinterpret_cast<const float*>(work.data())};
    sound.samples.assign(samples, samples + bytes / int(sizeof(float)));
    sound.sampleRate = spec.freq;
    return true;
}

bool SdlAudio::open()
{
    int frequency{};
    Uint16 format{};
    int channels{};
    if (!Mix_QuerySpec(&frequency, &format, &channels))
    {
        SDL_Log("Mix_QuerySpec failed: %s", Mix_GetError());
        return false;
    }
    if ((format != AUDIO_S16SYS && format != AUDIO_F32SYS) || channels < 1 || channels > 2)
    {
        SDL_Log("unsupported audio format: 0x%x, %d channels", format, channels);
        return false;
    }
    m_rate = frequency;
    m_format = format;
    m_channels = channels;
    m_buffer.assign(std::size_t(MAX_FRAMES) * 2, 0.0f);

    struct Entry
    {
        const char* file;
        bool loop;
        int id;
    };
    const Entry entries[]{
        {"engine_loop.wav", true, int(Loop::Engine)},        {"squeal_loop.wav", true, int(Loop::Squeal)},
        {"drift_start.wav", false, int(Effect::DriftStart)}, {"combo_up.wav", false, int(Effect::Combo)},
        {"drift_bank.wav", false, int(Effect::Bank)},        {"crash.wav", false, int(Effect::Crash)},
        {"lamp_on.wav", false, int(Effect::LampOn)},         {"cone_hit.wav", false, int(Effect::ConeHit)},
        {"drum_hit.wav", false, int(Effect::DrumHit)},       {"port_lit.wav", false, int(Effect::PortLit)},
    };
    bool ok{true};
    for (const Entry& entry : entries)
    {
        Sound sound;
        if (!load(entry.file, sound))
        {
            ok = false;
            continue;
        }
        if (entry.loop)
        {
            m_mixer.setSound(Loop(entry.id), std::move(sound));
        }
        else
        {
            m_mixer.setSound(Effect(entry.id), std::move(sound));
        }
    }
    Mix_HookMusic(&SdlAudio::fill, this);
    m_open = true;
    return ok;
}

void SdlAudio::close()
{
    if (m_open)
    {
        Mix_HookMusic(nullptr, nullptr);
        m_open = false;
    }
}

void SdlAudio::fill(void* self, Uint8* stream, int length)
{
    SdlAudio& audio{*static_cast<SdlAudio*>(self)};
    int sampleBytes{audio.m_format == AUDIO_F32SYS ? 4 : 2};
    int frames{length / (sampleBytes * audio.m_channels)};
    int written{};
    while (written < frames)
    {
        int count{std::min(frames - written, MAX_FRAMES)};
        audio.m_mixer.render(audio.m_buffer.data(), count, audio.m_rate);
        for (int i{}; i < count; ++i)
        {
            for (int c{}; c < audio.m_channels; ++c)
            {
                float value{audio.m_buffer[std::size_t(i) * 2 + std::size_t(c)]};
                std::size_t index{std::size_t(written + i) * std::size_t(audio.m_channels) + std::size_t(c)};
                if (audio.m_format == AUDIO_F32SYS)
                {
                    reinterpret_cast<float*>(stream)[index] = value;
                }
                else
                {
                    reinterpret_cast<Sint16*>(stream)[index] = Sint16(std::clamp(value, -1.0f, 1.0f) * 32767.0f);
                }
            }
        }
        written += count;
    }
}

}  // namespace adapters::sdl
