#include "adapters/gl/TrackMesh.hpp"

#include <algorithm>
#include <cmath>

namespace {

Vec3 horizontalRight(const Track& track, int index) {
    const int n = static_cast<int>(track.samples.size());
    const TrackSample& prev = track.samples[(index + n - 1) % n];
    const TrackSample& next = track.samples[(index + 1) % n];
    float tx = next.x - prev.x;
    float tz = next.z - prev.z;
    float len = std::sqrt(tx * tx + tz * tz);
    if (len < 1e-4f) {
        return {1, 0, 0};
    }
    tx /= len;
    tz /= len;
    return {tz, 0, -tx};
}

Vec3 roadNormal(const Track& track, int index) {
    const int n = static_cast<int>(track.samples.size());
    const TrackSample& prev = track.samples[(index + n - 1) % n];
    const TrackSample& next = track.samples[(index + 1) % n];
    Vec3 tangent = normalize(Vec3{next.x - prev.x, next.y - prev.y, next.z - prev.z});
    Vec3 right = horizontalRight(track, index);
    right.y = track.samples[index].cross;
    right = normalize(right);
    Vec3 normal = normalize(cross(tangent, right));
    if (normal.y < 0) {
        normal = normal * -1.0f;
    }
    return normal;
}

Vec3 onRoad(const Track& track, int index, float lateral) {
    const TrackSample& sample = track.samples[index];
    Vec3 right = horizontalRight(track, index);
    return {
        sample.x + right.x * lateral,
        sample.y + lateral * sample.cross,
        sample.z + right.z * lateral,
    };
}

void addRibbon(CpuMesh& mesh, const Track& track, float lat0, float lat1, Vec3 color, bool flatOuter) {
    const int n = static_cast<int>(track.samples.size());
    for (int i = 0; i < n; ++i) {
        int j = (i + 1) % n;
        Vec3 a = onRoad(track, i, lat0);
        Vec3 b = onRoad(track, i, lat1);
        Vec3 c = onRoad(track, j, lat1);
        Vec3 d = onRoad(track, j, lat0);
        if (flatOuter) {
            // 芝は縁石の外側の高さで水平にする。
            float edge = (std::fabs(lat0) > std::fabs(lat1) ? lat1 : lat0);
            float sign = edge >= 0 ? 1.0f : -1.0f;
            float edgeLat = sign * (track.halfWidth + track.curbWidth);
            float y0 = track.samples[i].y + edgeLat * track.samples[i].cross;
            float y1 = track.samples[j].y + edgeLat * track.samples[j].cross;
            a.y = y0;
            b.y = y0;
            c.y = y1;
            d.y = y1;
        }
        Vec3 normal = normalize(roadNormal(track, i) + roadNormal(track, j));
        Vec3 tint = color;
        if (!flatOuter) {
            float shade = 0.94f + 0.08f * float((i * 13) % 5) / 5.0f;
            tint = color * shade;
        }
        mesh.addQuad(a, b, c, d, normal, tint);
    }
}

void addWalls(CpuMesh& mesh, const Track& track) {
    const int n = static_cast<int>(track.samples.size());
    const float inner = track.wallLateral();
    const float outer = inner + 0.4f;
    const float height = 0.9f;
    const Vec3 concrete{0.62f, 0.63f, 0.66f};
    for (int side = -1; side <= 1; side += 2) {
        float sign = float(side);
        for (int i = 0; i < n; ++i) {
            int j = (i + 1) % n;
            Vec3 a = onRoad(track, i, sign * inner);
            Vec3 b = onRoad(track, j, sign * inner);
            Vec3 c = onRoad(track, j, sign * outer);
            Vec3 d = onRoad(track, i, sign * outer);
            // 芝の高さに合わせる
            float edge = sign * (track.halfWidth + track.curbWidth);
            a.y = track.samples[i].y + edge * track.samples[i].cross;
            b.y = track.samples[j].y + edge * track.samples[j].cross;
            c.y = b.y;
            d.y = a.y;
            Vec3 topA = a + Vec3{0, height, 0};
            Vec3 topB = b + Vec3{0, height, 0};
            Vec3 topC = c + Vec3{0, height, 0};
            Vec3 topD = d + Vec3{0, height, 0};
            Vec3 inward{-sign * horizontalRight(track, i).x, 0, -sign * horizontalRight(track, i).z};
            mesh.addQuad(a, b, topB, topA, inward, concrete);
            mesh.addQuad(d, topD, topC, c, inward * -1.0f, concrete * 0.85f);
            mesh.addQuad(topA, topB, topC, topD, {0, 1, 0}, concrete * 1.05f);
        }
    }
}

void addCurbs(CpuMesh& mesh, const Track& track) {
    const int n = static_cast<int>(track.samples.size());
    const Vec3 red{0.75f, 0.09f, 0.08f};
    const Vec3 white{0.90f, 0.90f, 0.92f};
    for (int side = -1; side <= 1; side += 2) {
        float sign = float(side);
        float inner = sign * track.halfWidth;
        float outer = sign * (track.halfWidth + track.curbWidth);
        for (int i = 0; i < n; ++i) {
            int j = (i + 1) % n;
            bool paint = (static_cast<int>(track.samples[i].s / 3.0f) % 2) == 0;
            Vec3 color = paint ? red : white;
            Vec3 a = onRoad(track, i, inner);
            Vec3 b = onRoad(track, i, outer);
            Vec3 c = onRoad(track, j, outer);
            Vec3 d = onRoad(track, j, inner);
            Vec3 normal = normalize(roadNormal(track, i) + roadNormal(track, j));
            mesh.addQuad(a, b, c, d, normal, color);
        }
    }
}

void addStartLine(CpuMesh& mesh, const Track& track) {
    if (track.samples.size() < 2) {
        return;
    }
    Vec3 right = horizontalRight(track, 0);
    float yaw = track.spawnYaw;
    // スポーンはラインの手前なので、ライン自体はサンプル 0 の接線を使う。
    const TrackSample& a = track.samples[0];
    const TrackSample& b = track.samples[1];
    float tx = b.x - a.x;
    float tz = b.z - a.z;
    float len = std::sqrt(tx * tx + tz * tz);
    if (len > 1e-4f) {
        tx /= len;
        tz /= len;
        yaw = std::atan2(tx, tz);
    }
    Vec3 forward{std::sin(yaw), 0, std::cos(yaw)};
    Vec3 center{a.x, a.y, a.z};
    const int cells = 8;
    const float depth = 0.8f;
    for (int i = 0; i < cells; ++i) {
        float lat0 = -track.halfWidth + track.halfWidth * 2.0f * (float(i) / cells);
        float lat1 = -track.halfWidth + track.halfWidth * 2.0f * (float(i + 1) / cells);
        Vec3 color = (i % 2 == 0) ? Vec3{0.92f, 0.92f, 0.94f} : Vec3{0.05f, 0.05f, 0.06f};
        Vec3 p0 = center + right * lat0;
        Vec3 p1 = center + right * lat1;
        Vec3 p2 = p1 + forward * depth;
        Vec3 p3 = p0 + forward * depth;
        p0.y = a.y + lat0 * a.cross + 0.04f;
        p1.y = a.y + lat1 * a.cross + 0.04f;
        p2.y = p1.y;
        p3.y = p0.y;
        mesh.addQuad(p0, p1, p2, p3, {0, 1, 0}, color);
    }

    // スタートの門。位置が分かるようにする。
    const float pillar = track.halfWidth + 1.1f;
    const Vec3 steel{0.22f, 0.24f, 0.28f};
    for (int side = -1; side <= 1; side += 2) {
        Vec3 base = center + right * (float(side) * pillar);
        base.y = a.y;
        Vec3 axisX = right * 0.28f;
        Vec3 axisZ = forward * 0.28f;
        Vec3 y0 = base;
        Vec3 y1 = base + Vec3{0, 6.4f, 0};
        mesh.addQuad(y0 - axisX - axisZ, y0 + axisX - axisZ, y1 + axisX - axisZ, y1 - axisX - axisZ, forward * -1.0f, steel);
        mesh.addQuad(y0 - axisX + axisZ, y1 - axisX + axisZ, y1 + axisX + axisZ, y0 + axisX + axisZ, forward, steel);
        mesh.addQuad(y0 - axisX - axisZ, y1 - axisX - axisZ, y1 - axisX + axisZ, y0 - axisX + axisZ, right * -1.0f, steel);
        mesh.addQuad(y0 + axisX - axisZ, y0 + axisX + axisZ, y1 + axisX + axisZ, y1 + axisX - axisZ, right, steel);
    }
    Vec3 left = center + right * -pillar + Vec3{0, 6.2f, 0};
    Vec3 rightTop = center + right * pillar + Vec3{0, 6.2f, 0};
    mesh.addQuad(left, rightTop, rightTop + Vec3{0, 0.4f, 0}, left + Vec3{0, 0.4f, 0}, forward, steel);
}

void addPit(CpuMesh& mesh, const std::vector<Vec3>& pit) {
    if (pit.size() < 2) {
        return;
    }
    const float half = 4.0f;
    const Vec3 color{0.24f, 0.24f, 0.26f};
    for (size_t i = 0; i + 1 < pit.size(); ++i) {
        Vec3 a = pit[i];
        Vec3 b = pit[i + 1];
        float tx = b.x - a.x;
        float tz = b.z - a.z;
        float len = std::sqrt(tx * tx + tz * tz);
        if (len < 0.05f) {
            continue;
        }
        tx /= len;
        tz /= len;
        Vec3 right{tz, 0, -tx};
        Vec3 a0 = a + right * -half;
        Vec3 a1 = a + right * half;
        Vec3 b1 = b + right * half;
        Vec3 b0 = b + right * -half;
        a0.y = a.y + 0.03f;
        a1.y = a.y + 0.03f;
        b0.y = b.y + 0.03f;
        b1.y = b.y + 0.03f;
        mesh.addQuad(a0, a1, b1, b0, {0, 1, 0}, color);
    }
}

void addGround(CpuMesh& mesh, const Track& track) {
    float minX = track.samples[0].x;
    float maxX = minX;
    float minZ = track.samples[0].z;
    float maxZ = minZ;
    float minY = track.samples[0].y;
    for (const TrackSample& sample : track.samples) {
        minX = std::min(minX, sample.x);
        maxX = std::max(maxX, sample.x);
        minZ = std::min(minZ, sample.z);
        maxZ = std::max(maxZ, sample.z);
        minY = std::min(minY, sample.y);
    }
    const float pad = 120.0f;
    float y = minY - 4.0f;
    Vec3 color{0.12f, 0.27f, 0.11f};
    Vec3 a{minX - pad, y, minZ - pad};
    Vec3 b{maxX + pad, y, minZ - pad};
    Vec3 c{maxX + pad, y, maxZ + pad};
    Vec3 d{minX - pad, y, maxZ + pad};
    mesh.addQuad(a, d, c, b, {0, 1, 0}, color);
}

}  // namespace

void buildTrackMesh(const Track& track, CpuMesh& mesh) {
    mesh = CpuMesh{};
    addGround(mesh, track);
    const float curbOuter = track.halfWidth + track.curbWidth;
    const float grassOuter = curbOuter + track.runoff;
    addRibbon(mesh, track, -grassOuter, -curbOuter, {0.15f, 0.36f, 0.14f}, true);
    addRibbon(mesh, track, curbOuter, grassOuter, {0.15f, 0.36f, 0.14f}, true);
    addRibbon(mesh, track, -track.halfWidth, track.halfWidth, {0.17f, 0.17f, 0.19f}, false);
    addCurbs(mesh, track);
    addWalls(mesh, track);
    addPit(mesh, track.pit);
    addStartLine(mesh, track);
}
