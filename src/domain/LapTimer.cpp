#include "domain/LapTimer.hpp"

void LapTimer::tick(float s, float length, float dt) {
    if (prevS >= 0) {
        float gap = (length - prevS) + s;
        if (prevS > length * 0.5f && s < length * 0.5f && gap < 160.0f) {
            if (timing) {
                if (best < 0 || lap < best) {
                    best = lap;
                }
                completed += 1;
            }
            timing = true;
            lap = 0;
        }
    }
    prevS = s;
    lap += dt;
}
