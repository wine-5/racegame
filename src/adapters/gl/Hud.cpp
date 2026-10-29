#include "adapters/gl/Hud.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace adapters::gl
{

using application::GameSession;
using domain::Car;
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
    int minutes = static_cast<int>(seconds / 60.0f);
    float rest = seconds - float(minutes) * 60.0f;
    std::snprintf(buf, size, "%d:%06.3f", minutes, rest);
}

void drawMinimap(Renderer& renderer, const Track& track, const Car& car)
{
    float size = 190.0f;
    float x0 = 24.0f;
    float y0 = float(renderer.height) - size - 28.0f;
    renderer.addQuad(x0, y0, x0 + size, y0 + size, 0.02f, 0.03f, 0.05f, 0.78f);
    float minX = track.samples[0].x;
    float maxX = minX;
    float minZ = track.samples[0].z;
    float maxZ = minZ;
    for (const TrackSample& sample : track.samples)
    {
        minX = std::min(minX, sample.x);
        maxX = std::max(maxX, sample.x);
        minZ = std::min(minZ, sample.z);
        maxZ = std::max(maxZ, sample.z);
    }
    float midX = (minX + maxX) * 0.5f;
    float midZ = (minZ + maxZ) * 0.5f;
    float span = std::max(maxX - minX, maxZ - minZ) * 1.12f;
    auto mapX = [&](float x) { return x0 + size * 0.5f + (x - midX) / span * (size - 18.0f); };
    auto mapY = [&](float z) { return y0 + size * 0.5f - (z - midZ) / span * (size - 18.0f); };
    const int n = static_cast<int>(track.samples.size());
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
    float mx = mapX(car.x);
    float my = mapY(car.z);
    Vec3 nose{std::sin(car.yaw), 0, std::cos(car.yaw)};
    Vec3 side{std::cos(car.yaw), 0, -std::sin(car.yaw)};
    float nx = mx + nose.x * 9.0f;
    float ny = my - nose.z * 9.0f;
    float lx = mx - nose.x * 5.0f + side.x * 5.0f;
    float ly = my + nose.z * 5.0f - side.z * 5.0f;
    float rx = mx - nose.x * 5.0f - side.x * 5.0f;
    float ry = my + nose.z * 5.0f + side.z * 5.0f;
    renderer.drawLine(lx, ly, nx, ny, 2.0f, 0.9f, 0.2f, 0.18f, 1);
    renderer.drawLine(rx, ry, nx, ny, 2.0f, 0.9f, 0.2f, 0.18f, 1);
    renderer.drawLine(lx, ly, rx, ry, 2.0f, 0.9f, 0.2f, 0.18f, 1);
}

}  // namespace

void drawHud(Renderer& renderer, const GameSession& session, const ChaseCamera& camera, bool showHelp)
{
    Renderer& hud = renderer;
    hud.beginHud();
    char line[96];
    int kmh = static_cast<int>(std::lround(session.car().speed * 3.6f));
    std::snprintf(line, sizeof(line), "%d KM/H", kmh);
    hud.drawText(24, 22, 4.0f, line, 0.95f, 0.96f, 0.98f, 1);

    if (!session.lap().timing)
    {
        std::snprintf(line, sizeof(line), "OUT LAP");
    }
    else
    {
        std::snprintf(line, sizeof(line), "LAP %d", session.lap().completed + 1);
    }
    hud.drawText(24, 64, 2.2f, line, 0.85f, 0.88f, 0.92f, 1);
    formatTime(line, sizeof(line), session.lap().lap);
    hud.drawText(150, 64, 2.2f, line, 0.95f, 0.96f, 0.98f, 1);

    char best[32];
    formatTime(best, sizeof(best), session.lap().best);
    std::snprintf(line, sizeof(line), "BEST %s", best);
    hud.drawText(24, 90, 2.0f, line, 0.75f, 0.8f, 0.7f, 1);

    int percent = static_cast<int>(session.car().ground.s / session.track().length * 100.0f);
    const char* corner = session.track().nameOf(session.car().ground.nameId);
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

    hud.drawText(float(hud.width) - 140, 22, 2.0f, camera.modeName(), 0.8f, 0.84f, 0.88f, 1);

    if (showHelp)
    {
        hud.drawText(24, float(hud.height) - 78, 1.7f,
                     "W/UP ACCEL  S/DOWN BRAKE  A/D STEER  R RESET  C CAMERA  P PAUSE", 0.9f, 0.92f, 0.94f, 1);
        hud.drawText(24, float(hud.height) - 54, 1.7f,
                     "PAD: RT ACCEL  LT BRAKE  STICK STEER  Y CAMERA  BACK RESET  START PAUSE", 0.75f, 0.8f, 0.84f, 1);
    }
    hud.drawText(float(hud.width) - 250, float(hud.height) - 28, 1.6f, "(C) OSM / GSI", 0.85f, 0.88f, 0.9f, 1);
    if (session.paused())
    {
        hud.drawText(float(hud.width) * 0.5f - 70, float(hud.height) * 0.45f, 5.0f, "PAUSE", 1, 0.95f, 0.9f, 1);
    }
    drawMinimap(renderer, session.track(), session.car());
    hud.flushHud();
}

}  // namespace adapters::gl
