#include "domain/track.hpp"

#include <cmath>
#include <fstream>
#include <sstream>

bool Track::load(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        return false;
    }

    enum class Mode { Top, Names, Samples, Pit };
    Mode mode = Mode::Top;
    int expectNames = 0;
    int expectSamples = 0;
    int expectPit = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::istringstream ss(line);
        if (mode == Mode::Names) {
            int id = 0;
            ss >> id;
            std::string name;
            std::getline(ss, name);
            if (!name.empty() && name[0] == ' ') {
                name.erase(0, 1);
            }
            names.push_back(name);
            if (static_cast<int>(names.size()) >= expectNames) {
                mode = Mode::Top;
            }
            continue;
        }
        if (mode == Mode::Samples) {
            TrackSample sample;
            int bridge = 0;
            ss >> sample.x >> sample.z >> sample.y >> sample.cross >> bridge >> sample.corner >> sample.nameId;
            if (ss.fail()) {
                return false;
            }
            sample.bridge = bridge != 0;
            samples.push_back(sample);
            if (static_cast<int>(samples.size()) >= expectSamples) {
                mode = Mode::Top;
            }
            continue;
        }
        if (mode == Mode::Pit) {
            Vec3 point;
            ss >> point.x >> point.z >> point.y;
            if (ss.fail()) {
                return false;
            }
            pit.push_back(point);
            if (static_cast<int>(pit.size()) >= expectPit) {
                mode = Mode::Top;
            }
            continue;
        }

        std::string key;
        ss >> key;
        if (key == "origin") {
            ss >> originLat >> originLon;
        } else if (key == "half_width") {
            ss >> halfWidth;
        } else if (key == "curb_width") {
            ss >> curbWidth;
        } else if (key == "length") {
            ss >> length;
        } else if (key == "spawn") {
            ss >> spawnX >> spawnY >> spawnZ >> spawnYaw;
        } else if (key == "names") {
            ss >> expectNames;
            mode = Mode::Names;
        } else if (key == "samples") {
            ss >> expectSamples;
            mode = Mode::Samples;
        } else if (key == "pit") {
            ss >> expectPit;
            mode = expectPit > 0 ? Mode::Pit : Mode::Top;
        }
    }

    if (samples.size() < 16) {
        return false;
    }

    length = 0;
    for (size_t i = 0; i < samples.size(); ++i) {
        samples[i].s = length;
        const TrackSample& a = samples[i];
        const TrackSample& b = samples[(i + 1) % samples.size()];
        length += std::sqrt((b.x - a.x) * (b.x - a.x) + (b.z - a.z) * (b.z - a.z));
    }

    return true;
}

TrackPose Track::query(float x, float z, float yHint, float preferS) const {
    TrackPose best;
    float bestScore = 1.0e30f;
    const int n = static_cast<int>(samples.size());
    for (int i = 0; i < n; ++i) {
        const TrackSample& a = samples[i];
        const TrackSample& b = samples[(i + 1) % n];
        float abx = b.x - a.x;
        float abz = b.z - a.z;
        float len2 = abx * abx + abz * abz;
        if (len2 < 1e-4f) {
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
        if (preferS >= 0.0f && length > 1.0f) {
            float along = a.s + std::sqrt(len2) * t;
            float ds = std::fabs(along - preferS);
            if (ds > length * 0.5f) {
                ds = length - ds;
            }
            // 今走っている周回位置から遠い路面は、交差の真下でも選ばない。
            score += ds * 0.2f;
        }
        if (score >= bestScore) {
            continue;
        }
        bestScore = score;
        float invLen = 1.0f / std::sqrt(len2);
        float tx = abx * invLen;
        float tz = abz * invLen;
        best.found = true;
        best.s = a.s + std::sqrt(len2) * t;
        if (best.s >= length) {
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

const char* Track::nameOf(int nameId) const {
    if (nameId < 0 || nameId >= static_cast<int>(names.size())) {
        return "";
    }
    return names[nameId].c_str();
}
