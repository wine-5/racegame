#pragma once

#include "domain/track.hpp"

// ポート: コースをどこから持ってくるか。
// 中心はこのインターフェースだけを知り、ファイル・埋め込みデータ・テスト用の
// 手書きコースなどの具体的な読み方はアダプター側が実装する。
class TrackSource {
public:
    virtual ~TrackSource() = default;
    // 成功すると track を埋めて true を返す。
    virtual bool load(Track& track) = 0;
};
