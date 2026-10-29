#include "adapters/course/PortCourse.hpp"

#include "domain/CourseBuilder.hpp"

namespace adapters::course
{

namespace
{

// 一周がきれいに閉じるように、海沿いの直線と倉庫の間の直線の長さを計算で決めてある。
// 部品を変えるときは、この 2 つを解き直す（どちらも長さに対して位置が線形に変わる）
constexpr float QUAY_LENGTH = 211.885f;
constexpr float WAREHOUSE_LENGTH = 46.0f;

}  // namespace

bool PortCourse::load(domain::Track& track)
{
    domain::CourseBuilder builder{0.0f, 0.0f, 0.0f, 12.0f};

    builder.section("QUAYSIDE");
    builder.straight(QUAY_LENGTH, 12.0f);

    // クレーンの下の広場。幅 40m の大きなヘアピンで、深い角度のまま回り込める
    builder.section("CRANE HAIRPIN");
    builder.arc(56.0f, 180.0f, 20.0f);
    builder.straight(40.0f, 10.0f);

    // コンテナの間の細い道。左右に 3 回切り返すので倍率を上げやすい
    builder.section("CONTAINER ALLEY");
    builder.arc(30.0f, -60.0f, 8.0f);
    builder.arc(30.0f, 120.0f, 8.0f);
    builder.arc(30.0f, -120.0f, 8.0f);
    builder.arc(30.0f, 60.0f, 8.0f);
    builder.straight(40.0f, 9.0f);

    builder.section("WAREHOUSE CORNER");
    builder.arc(36.0f, 90.0f, 11.0f);
    builder.straight(WAREHOUSE_LENGTH, 11.0f);

    // 長い一定半径のカーブ。角度を保ったまま海沿いの直線へ戻る
    builder.section("DOCK SWEEPER");
    builder.arc(60.0f, 90.0f, 12.0f);

    // 壁はコンテナで、道のすぐ脇に立つ
    track.curbWidth = 0.6f;
    track.runoff = 1.5f;
    return builder.build(track);
}

}  // namespace adapters::course
