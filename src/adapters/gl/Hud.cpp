#include "adapters/gl/Hud.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace adapters::gl
{

using application::GameSession;
using domain::Car;
using domain::DriftScore;
using domain::Track;
using domain::TrackSample;
using domain::Vec3;

namespace
{

void formatTime(char* buf, int size, float seconds)
{
    if (seconds < 0)
    {
        std::snprintf(buf, size, "--:--.---");
        return;
    }
    int minutes{static_cast<int>(seconds / 60.0f)};
    float rest{seconds - float(minutes) * 60.0f};
    std::snprintf(buf, size, "%d:%06.3f", minutes, rest);
}

void drawMinimap(Renderer& renderer, const Track& track, const Car& car)
{
    float size{190.0f};
    float x0{24.0f};
    float y0{float(renderer.height()) - size - 28.0f};
    renderer.addQuad(x0, y0, x0 + size, y0 + size, 0.02f, 0.03f, 0.05f, 0.78f);
    float minX{track.samples[0].x};
    float maxX{minX};
    float minZ{track.samples[0].z};
    float maxZ{minZ};
    for (const TrackSample& sample : track.samples)
    {
        minX = std::min(minX, sample.x);
        maxX = std::max(maxX, sample.x);
        minZ = std::min(minZ, sample.z);
        maxZ = std::max(maxZ, sample.z);
    }
    float midX{(minX + maxX) * 0.5f};
    float midZ{(minZ + maxZ) * 0.5f};
    float span{std::max(maxX - minX, maxZ - minZ) * 1.12f};
    auto mapX = [&](float x) { return x0 + size * 0.5f + (x - midX) / span * (size - 18.0f); };
    auto mapY = [&](float z) { return y0 + size * 0.5f - (z - midZ) / span * (size - 18.0f); };
    const int n{static_cast<int>(track.samples.size())};
    for (int i = 0; i < n; ++i)
    {
        const TrackSample& a = track.samples[i];
        const TrackSample& b = track.samples[(i + 1) % n];
        float r = 0.75f, g = 0.78f, bl = 0.82f;
        if (a.bridge || b.bridge)
        {
            r = 0.95f;
            g = 0.72f;
            bl = 0.28f;
        }
        renderer.drawLine(mapX(a.x), mapY(a.z), mapX(b.x), mapY(b.z), 2.0f, r, g, bl, 1);
    }
    float mx{mapX(car.x)};
    float my{mapY(car.z)};
    Vec3 nose{std::sin(car.yaw), 0, std::cos(car.yaw)};
    Vec3 side{std::cos(car.yaw), 0, -std::sin(car.yaw)};
    float nx{mx + nose.x * 9.0f};
    float ny{my - nose.z * 9.0f};
    float lx{mx - nose.x * 5.0f + side.x * 5.0f};
    float ly{my + nose.z * 5.0f - side.z * 5.0f};
    float rx{mx - nose.x * 5.0f - side.x * 5.0f};
    float ry{my + nose.z * 5.0f + side.z * 5.0f};
    renderer.drawLine(lx, ly, nx, ny, 2.0f, 0.9f, 0.2f, 0.18f, 1);
    renderer.drawLine(rx, ry, nx, ny, 2.0f, 0.9f, 0.2f, 0.18f, 1);
    renderer.drawLine(lx, ly, rx, ry, 2.0f, 0.9f, 0.2f, 0.18f, 1);
}

// 内蔵フォントで描いたときの文字列の幅（Renderer::drawText と同じ送り幅）
float textWidth(const char* text, float scale)
{
    float width{};
    for (const char* cursor{text}; *cursor; ++cursor)
    {
        width += (*cursor == ' ' ? 4.0f : 6.0f) * scale;
    }
    return width;
}

void drawCentered(Renderer& renderer, float y, float scale, const char* text, float r, float g, float b, float a)
{
    float x{(float(renderer.width()) - textWidth(text, scale)) * 0.5f};
    renderer.drawText(x, y, scale, text, r, g, b, a);
}

// 画面上部の中央に、ドリフトの点・倍率・角度と、終わった連続の結果を出す
void drawDrift(Renderer& renderer, const DriftScore& drift, const Car& car)
{
    constexpr float RESULT_SHOW_SEC = 2.0f;
    constexpr float RAD_TO_DEG = 57.29578f;
    constexpr float GAUGE_MAX_DEG = 60.0f;
    char line[64];

    std::snprintf(line, sizeof(line), "SCORE %lld", drift.total());
    renderer.drawText(float(renderer.width()) - textWidth(line, 2.2f) - 24, 56, 2.2f, line, 0.95f, 0.85f, 0.55f, 1);

    if (drift.chaining())
    {
        // 倍率が上がるほど、白から橙へ色を変える
        float heat{float(drift.multiplier() - 1) / 4.0f};
        float r{1.0f};
        float g{0.95f - 0.45f * heat};
        float b{0.85f - 0.75f * heat};
        float alpha{drift.drifting() ? 1.0f : 0.55f};
        std::snprintf(line, sizeof(line), "%d", static_cast<int>(drift.chainPoints()));
        drawCentered(renderer, 28, 5.0f, line, r, g, b, alpha);
        // 倍率が上がった直後は大きく弾ませる
        float pop{1.0f + 0.8f * std::exp(-drift.sinceMultiplierUp() * 7.0f)};
        float scale{3.0f * pop};
        std::snprintf(line, sizeof(line), "X%d", drift.multiplier());
        drawCentered(renderer, 72 - (scale - 3.0f) * 3.5f, scale, line, r, g, b, alpha);

        // 角度のゲージ
        float angle{std::fabs(car.slipAngle()) * RAD_TO_DEG};
        float fill{std::fmin(angle / GAUGE_MAX_DEG, 1.0f)};
        float width{240.0f};
        float x0{(float(renderer.width()) - width) * 0.5f};
        renderer.addQuad(x0, 104, x0 + width, 112, 0.05f, 0.05f, 0.07f, 0.6f * alpha);
        renderer.addQuad(x0, 104, x0 + width * fill, 112, r, g, b, alpha);
        std::snprintf(line, sizeof(line), "%d DEG", static_cast<int>(angle));
        drawCentered(renderer, 118, 1.8f, line, 0.9f, 0.9f, 0.92f, alpha);
        return;
    }

    if (drift.lastResult() != DriftScore::Result::None && drift.sinceLastResult() < RESULT_SHOW_SEC)
    {
        float alpha{1.0f - drift.sinceLastResult() / RESULT_SHOW_SEC};
        if (drift.lastResult() == DriftScore::Result::Banked)
        {
            std::snprintf(line, sizeof(line), "%lld", drift.lastPoints());
            drawCentered(renderer, 28, 5.0f, line, 0.55f, 1.0f, 0.6f, alpha);
            drawCentered(renderer, 72, 2.4f, "NICE DRIFT", 0.55f, 1.0f, 0.6f, alpha);
        }
        else
        {
            drawCentered(renderer, 28, 5.0f, "CRASH", 1.0f, 0.35f, 0.3f, alpha);
        }
    }
}

}  // namespace

void drawTitle(Renderer& renderer, const TitleScreen& title, float time)
{
    renderer.beginHud();
    const float w{float(renderer.width())};
    const float h{float(renderer.height())};
    // 画面全体を少し暗くし、上下に帯を入れて映画のように見せる
    renderer.addQuad(0, 0, w, h, 0.0f, 0.0f, 0.02f, 0.35f);
    renderer.addQuad(0, 0, w, h * 0.09f, 0, 0, 0, 0.85f);
    renderer.addQuad(0, h * 0.91f, w, h, 0, 0, 0, 0.85f);

    // 題名。少しずらして重ね描きし、ネオンのにじみに見せる
    const char* name{"NIGHT PORT DRIFT"};
    float scale{std::fmin(w / 140.0f, 11.0f)};
    float y{h * 0.24f};
    for (int i{3}; i >= 1; --i)
    {
        float spread{float(i) * scale * 0.35f};
        float x{(w - textWidth(name, scale)) * 0.5f};
        renderer.drawText(x - spread, y, scale, name, 1.0f, 0.15f, 0.65f, 0.10f);
        renderer.drawText(x + spread, y, scale, name, 0.2f, 0.6f, 1.0f, 0.10f);
    }
    drawCentered(renderer, y, scale, name, 1.0f, 0.86f, 0.96f, 1.0f);
    float lineY{y + scale * 9.0f};
    float lineW{textWidth(name, scale) * 0.8f};
    renderer.addQuad((w - lineW) * 0.5f, lineY, (w + lineW) * 0.5f, lineY + 3, 0.2f, 0.9f, 1.0f, 0.9f);
    drawCentered(renderer, lineY + 18, scale * 0.28f, "WASD TO DRIVE  HOLD SPACE TO DRIFT", 0.75f, 0.85f, 1.0f, 0.9f);

    // 始め方。ゆっくり点滅させる
    float blink{0.55f + 0.45f * std::sin(time * 4.0f)};
    drawCentered(renderer, h * 0.62f, scale * 0.42f, "PRESS SPACE / CLICK TO START", 1.0f, 0.95f, 0.6f, blink);

    // ステージの一覧
    float listY{h * 0.74f};
    float itemScale{scale * 0.3f};
    float gap{itemScale * 6.0f * 10.0f};
    float total{gap * float(title.stages.size())};
    float x0{(w - total) * 0.5f};
    char line[64];
    for (std::size_t i{}; i < title.stages.size(); ++i)
    {
        bool chosen{static_cast<int>(i) == title.selected};
        std::snprintf(line, sizeof(line), "%d %s", static_cast<int>(i) + 1, title.stages[i].c_str());
        float x{x0 + gap * float(i) + (gap - textWidth(line, itemScale)) * 0.5f};
        if (chosen)
        {
            renderer.addQuad(x - 10, listY - 8, x + textWidth(line, itemScale) + 6, listY + itemScale * 7 + 8, 1.0f,
                             0.2f, 0.65f, 0.35f);
        }
        renderer.drawText(x, listY, itemScale, line, 1, 1, 1, chosen ? 1.0f : 0.5f);
    }
    drawCentered(renderer, h * 0.93f + 4, scale * 0.22f, "1-3 STAGE   F FULLSCREEN   ESC TITLE", 0.7f, 0.75f, 0.85f,
                 0.8f);
    renderer.flushHud();
}

void drawHud(Renderer& renderer, const GameSession& session, const ChaseCamera& camera, bool showHelp, bool showCredit)
{
    Renderer& hud = renderer;
    hud.beginHud();
    char line[96];
    int kmh{static_cast<int>(std::lround(session.car().speed * 3.6f))};
    std::snprintf(line, sizeof(line), "%d KM/H", kmh);
    hud.drawText(24, 22, 4.0f, line, 0.95f, 0.96f, 0.98f, 1);

    if (!session.lap().timing())
    {
        std::snprintf(line, sizeof(line), "OUT LAP");
    }
    else
    {
        std::snprintf(line, sizeof(line), "LAP %d", session.lap().completed() + 1);
    }
    hud.drawText(24, 64, 2.2f, line, 0.85f, 0.88f, 0.92f, 1);
    formatTime(line, sizeof(line), session.lap().lapTime());
    hud.drawText(150, 64, 2.2f, line, 0.95f, 0.96f, 0.98f, 1);

    char best[32];
    formatTime(best, sizeof(best), session.lap().bestTime());
    std::snprintf(line, sizeof(line), "BEST %s", best);
    hud.drawText(24, 90, 2.0f, line, 0.75f, 0.8f, 0.7f, 1);

    int percent{static_cast<int>(session.car().ground.s / session.track().length * 100.0f)};
    const char* corner{session.track().nameOf(session.car().ground.nameId)};
    if (session.car().ground.corner > 0 && corner[0])
    {
        std::snprintf(line, sizeof(line), "T%d %s  %d%%", session.car().ground.corner, corner, percent);
    }
    else if (corner[0])
    {
        std::snprintf(line, sizeof(line), "%s  %d%%", corner, percent);
    }
    else
    {
        std::snprintf(line, sizeof(line), "%d%%", percent);
    }
    hud.drawText(24, 116, 2.0f, line, 0.9f, 0.9f, 0.92f, 1);

    hud.drawText(float(hud.width()) - 140, 22, 2.0f, camera.modeName(), 0.8f, 0.84f, 0.88f, 1);

    if (showHelp)
    {
        hud.drawText(24, float(hud.height()) - 78, 1.7f,
                     "WASD DRIVE  SPACE DRIFT  A/D DRIFT ANGLE    R RESET  C CAMERA  P PAUSE  F FULLSCREEN  1-3 STAGE",
                     0.9f, 0.92f, 0.94f, 1);
        hud.drawText(24, float(hud.height()) - 54, 1.7f,
                     "PAD: STICK STEER  RT GAS  LT BRAKE  A DRIFT  Y CAMERA  BACK RESET  START PAUSE", 0.75f, 0.8f,
                     0.84f, 1);
    }
    if (showCredit)
    {
        hud.drawText(float(hud.width()) - 250, float(hud.height()) - 28, 1.6f, "(C) OSM / GSI", 0.85f, 0.88f, 0.9f, 1);
    }
    if (session.paused())
    {
        hud.drawText(float(hud.width()) * 0.5f - 70, float(hud.height()) * 0.45f, 5.0f, "PAUSE", 1, 0.95f, 0.9f, 1);
    }
    drawDrift(renderer, session.drift(), session.car());
    drawMinimap(renderer, session.track(), session.car());
    hud.flushHud();
}

}  // namespace adapters::gl
