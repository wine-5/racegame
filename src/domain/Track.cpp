#include "domain/Track.hpp"

#include <cmath>

namespace domain
{

bool Track::finalize()
{
    if (samples.size() < 16)
    {
        return false;
    }

    length = 0;
    for (size_t i = 0; i < samples.size(); ++i)
    {
        samples[i].s = length;
        const TrackSample& a = samples[i];
        const TrackSample& b = samples[(i + 1) % samples.size()];
        length += std::sqrt((b.x - a.x) * (b.x - a.x) + (b.z - a.z) * (b.z - a.z));
    }
    return true;
}

TrackPose Track::query(float x, float z, float yHint, float preferS) const
{
    TrackPose best;
    float bestScore = 1.0e30f;
    const int n = static_cast<int>(samples.size());
    for (int i = 0; i < n; ++i)
    {
        const TrackSample& a = samples[i];
        const TrackSample& b = samples[(i + 1) % n];
        float abx = b.x - a.x;
        float abz = b.z - a.z;
        float len2 = abx * abx + abz * abz;
        if (len2 < 1e-4f)
        {
            continue;
        }
        float t = ((x - a.x) * abx + (z - a.z) * abz) / len2;
        t = clampf(t, 0.0f, 1.0f);
        float cx = a.x + abx * t;
        float cz = a.z + abz * t;
        float cy = a.y + (b.y - a.y) * t;
        float dx = x - cx;
        float dz = z - cz;
        float dy = yHint - cy;
        // 立体交差では平面距離より高さを優先する。6m 差なら、壁際にいても下の路面へ落ちない。
        float score = dx * dx + dz * dz + dy * dy * 12.0f;
        if (preferS >= 0.0f && length > 1.0f)
        {
            float along = a.s + std::sqrt(len2) * t;
            float ds = std::fabs(along - preferS);
            if (ds > length * 0.5f)
            {
                ds = length - ds;
            }
            // 今走っている周回位置から遠い路面は、交差の真下でも選ばない。
            score += ds * 0.2f;
        }
        if (score >= bestScore)
        {
            continue;
        }
        bestScore = score;
        float invLen = 1.0f / std::sqrt(len2);
        float tx = abx * invLen;
        float tz = abz * invLen;
        best.found = true;
        best.s = a.s + std::sqrt(len2) * t;
        if (best.s >= length)
        {
            best.s -= length;
        }
        best.rightX = tz;
        best.rightZ = -tx;
        best.lateral = dx * best.rightX + dz * best.rightZ;
        best.centerY = cy;
        best.cross = a.cross + (b.cross - a.cross) * t;
        best.yaw = std::atan2(tx, tz);
        best.xzDist = std::sqrt(dx * dx + dz * dz);
        best.corner = t < 0.5f ? a.corner : b.corner;
        best.nameId = t < 0.5f ? a.nameId : b.nameId;
    }
    return best;
}

const char* Track::nameOf(int nameId) const
{
    if (nameId < 0 || nameId >= static_cast<int>(names.size()))
    {
        return "";
    }
    return names[nameId].c_str();
}

}  // namespace domain
