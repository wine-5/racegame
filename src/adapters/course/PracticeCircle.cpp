#include "adapters/course/PracticeCircle.hpp"

#include "domain/CourseBuilder.hpp"

namespace adapters::course
{

bool PracticeCircle::load(domain::Track& track)
{
    constexpr float RADIUS = 34.0f;
    constexpr float HALF_WIDTH = 14.0f;
    domain::CourseBuilder builder{-RADIUS, 0.0f, 0.0f, HALF_WIDTH};
    builder.section("SKIDPAD");
    builder.arc(RADIUS, 360.0f, HALF_WIDTH);
    track.curbWidth = 0.6f;
    track.runoff = 3.0f;
    return builder.build(track);
}

}  // namespace adapters::course
