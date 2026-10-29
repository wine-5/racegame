#pragma once

#include "application/ports/track_source.hpp"

#include <string>

// assets/track/*.txt のテキスト形式からコースを読む。
class TrackFile : public TrackSource {
public:
    explicit TrackFile(std::string path);

    // 実行場所（build-native/、ブラウザの仮想ファイルシステム）によってパスが変わるので探す。
    static std::string findDefaultPath();

    bool load(Track& track) override;
    const std::string& path() const { return path_; }

private:
    std::string path_;
};
