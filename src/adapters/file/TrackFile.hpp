#pragma once

#include "application/ports/ITrackSource.hpp"

#include <string>

namespace adapters::file
{

// assets/track/*.txt のテキスト形式からコースを読む。
class TrackFile : public application::ports::ITrackSource
{
public:
    explicit TrackFile(std::string path);

    // 実行場所（build-native/、ブラウザの仮想ファイルシステム）によってパスが変わるので探す。
    static std::string findDefaultPath();

    bool load(domain::Track& track) override;
    const std::string& path() const { return m_path; }

private:
    std::string m_path;
};

}  // namespace adapters::file
