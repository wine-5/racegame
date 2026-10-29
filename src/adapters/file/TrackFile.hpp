#pragma once

#include "application/ports/ITrackSource.hpp"

#include <string>

namespace adapters::file
{

/**
 * @brief assets/track/*.txt のテキスト形式からコースを読む
 */
class TrackFile : public application::ports::ITrackSource
{
public:
    /**
     * @brief 読み込むファイルを指定して作る
     * @param path コースファイルのパス
     */
    explicit TrackFile(std::string path);

    /**
     * @brief 既定のコースファイルを探す
     * @details 実行場所（build-native/、ブラウザの仮想ファイルシステム）によってパスが変わるので探す。
     * @return 見つかったパス。見つからなければ最初の候補
     */
    static std::string findDefaultPath();

    /**
     * @brief コースを読み込む
     * @param track 読み込み先
     * @return ファイルが無い・形式が壊れているときは false
     */
    bool load(domain::Track& track) override;

    /// 読み込むファイルのパス
    const std::string& path() const { return m_path; }

private:
    std::string m_path;
};

}  // namespace adapters::file
