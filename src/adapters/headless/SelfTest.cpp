#include "adapters/headless/SelfTest.hpp"

#include "adapters/headless/Autopilot.hpp"
#include "domain/Car.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace adapters::headless
{

using domain::Car;
using domain::clampf;
using domain::DriveInput;
using domain::Track;
using domain::TrackPose;
using domain::wrapPi;

int runSelfTest(const Track& track)
{
    Car car;
    car.reset(track);
    float travelled{0};
    float prevS{-1};
    float maxLat{0};
    for (int step = 0; step < 60 * 50; ++step)
    {
        DriveInput input{autopilot(car, track)};
        car.update(1.0f / 60.0f, input, track);
        TrackPose pose{track.query(car.x, car.z, car.y, car.ground.found ? car.ground.s : -1.0f)};
        if (prevS >= 0)
        {
            float ds{pose.s - prevS};
            if (ds < -track.length * 0.5f)
                ds += track.length;
            if (ds > track.length * 0.5f)
                ds -= track.length;
            if (ds > 0)
                travelled += ds;
        }
        prevS = pose.s;
        maxLat = std::max(maxLat, pose.xzDist);
        if (!std::isfinite(car.x) || !std::isfinite(car.speed) || !std::isfinite(car.yaw))
        {
            std::printf("self-test non-finite\n");
            return 1;
        }
    }
    std::printf("self-test length %.0f travelled %.0f maxLat %.1f speed %.1f km/h\n", track.length, travelled, maxLat,
                car.speed * 3.6f);
    if (track.length < 5200.0f || track.length > 6500.0f)
        return 2;
    if (travelled < 1200.0f)
        return 3;
    if (maxLat > 30.0f)
        return 4;
    return 0;
}

}  // namespace adapters::headless
