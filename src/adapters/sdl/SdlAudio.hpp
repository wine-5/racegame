#pragma once

#include "adapters/audio/AudioMixer.hpp"

#include <SDL.h>

#include <string>
#include <vector>

namespace adapters::sdl
{

/**
 * @brief AudioMixer を SDL_mixer の出力につなぐ
 * @details assets/sound/ の WAV を読み込み、SDL_mixer の音楽の差し込み口（Mix_HookMusic）から
 *          AudioMixer が混ぜた音を流す。Mix_OpenAudio のあとに open、Mix_CloseAudio の前に close を呼ぶ。
 */
class SdlAudio
{
public:
    /**
     * @brief 鳴らす音を混ぜるものを指定して作る
     * @param mixer 音を混ぜるもの。SdlAudio より長く生きていること
     */
    explicit SdlAudio(audio::AudioMixer& mixer);

    /// close を呼んでいなければ呼ぶ
    ~SdlAudio();

    SdlAudio(const SdlAudio&) = delete;
    SdlAudio& operator=(const SdlAudio&) = delete;

    /**
     * @brief 音を読み込み、出力を始める
     * @return 読めない音があったり、出力の形式が扱えなければ false（音なしで続けられる）
     */
    bool open();

    /// 出力を止める
    void close();

private:
    /// Mix_HookMusic から呼ばれる
    static void fill(void* self, Uint8* stream, int length);

    /// WAV を読み込み、44.1 kHz のモノラルにする
    bool load(const std::string& name, audio::Sound& sound);

    audio::AudioMixer& m_mixer;
    bool m_open{};
    int m_rate{44100};
    Uint16 m_format{};
    int m_channels{2};
    std::vector<float> m_buffer;
};

}  // namespace adapters::sdl
