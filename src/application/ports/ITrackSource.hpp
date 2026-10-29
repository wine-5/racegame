#pragma once

#include "domain/Track.hpp"

namespace application::ports
{

// ポート: コースをどこから持ってくるか。
// 中心はこのインターフェースだけを知り、ファイル・埋め込みデータ・テスト用の
// 手書きコースなどの具体的な読み方はアダプター側が実装する。
class ITrackSource
{
public:
    virtual ~ITrackSource() = default;
    // 成功すると track を埋めて true を返す。
    virtual bool load(domain::Track& track) = 0;
};

}  // namespace application::ports
