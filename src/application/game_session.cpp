#include "application/game_session.hpp"

namespace {

constexpr double kMaxFrameSec = 0.25;
constexpr int kMaxStepsPerFrame = 8;

}  // namespace

bool GameSession::start(TrackSource& source) {
    track_ = Track{};
    if (!source.load(track_)) {
        return false;
    }
    reset();
    return true;
}

void GameSession::reset() {
    car_.reset(track_);
    lap_ = LapTimer{};
}

void GameSession::advance(double elapsedSec, const DriveInput& input) {
    if (elapsedSec > kMaxFrameSec) elapsedSec = kMaxFrameSec;
    if (elapsedSec < 0) elapsedSec = 0;

    if (paused_) {
        accumulator_ = 0;
        return;
    }
    accumulator_ += elapsedSec;
    int guard = 0;
    while (accumulator_ >= kFixedStepSec && guard < kMaxStepsPerFrame) {
        step(input);
        accumulator_ -= kFixedStepSec;
        ++guard;
    }
}

void GameSession::step(const DriveInput& input) {
    car_.update(float(kFixedStepSec), input, track_);
    lap_.tick(car_.ground.s, track_.length, float(kFixedStepSec));
}
