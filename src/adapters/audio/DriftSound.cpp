#include "adapters/audio/DriftSound.hpp"

#include <algorithm>
#include <cmath>

namespace adapters::audio
{

using domain::DriftScore;

namespace
{

// 各ギアで出せる速度の上限（m/s）。この速度で次のギアへ上げる
constexpr float GEAR_TOP[]{12.0f, 21.0f, 31.0f, 43.0f, 57.0f, 90.0f};
constexpr float IDLE_RPM = 900.0f;
constexpr float SHIFT_DOWN_RPM = 3600.0f;  // シフトアップした直後の回転数
constexpr float REDLINE_RPM = 7800.0f;
constexpr float SAMPLE_RPM = 1500.0f;   // engine_loop.wav の高さ（50 Hz、4 気筒の 1500 rpm）
constexpr float SPIN_RPM = 1800.0f;     // ドリフト中に後輪が空転して上がる回転数
constexpr float RPM_RESPONSE = 9.0f;    // 回転数が目標に追いつく速さ
constexpr float SQUEAL_START = 0.07f;   // この角度からスキール音が鳴り始める（ラジアン）
constexpr float SQUEAL_FULL = 0.45f;    // この角度で最大になる
constexpr float CRASH_COOLDOWN = 0.4f;  // 壁の音を続けて鳴らさない時間（秒）

float smoothstep(float x)
{
    x = std::clamp(x, 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

}  // namespace

DriftSound::DriftSound(AudioMixer& mixer) : m_mixer(mixer)
{
}

void DriftSound::reset()
{
    m_rpm = IDLE_RPM;
    m_wasChaining = false;
    m_lastMultiplier = 1;
    m_lastSinceResult = 1e9f;
    m_wasHitWall = false;
    m_crashCooldown = 0;
}

float DriftSound::targetRpm(const domain::Car& car, float throttle) const
{
    float speed{std::fabs(car.speed)};
    float low{};
    int gear{};
    while (gear < 5 && speed > GEAR_TOP[gear])
    {
        low = GEAR_TOP[gear];
        ++gear;
    }
    float ratio{std::clamp((speed - low) / (GEAR_TOP[gear] - low), 0.0f, 1.0f)};
    float bottom{gear == 0 ? IDLE_RPM + 1600.0f * throttle : SHIFT_DOWN_RPM};
    float rpm{bottom + (REDLINE_RPM - bottom) * ratio};
    // 横に滑っているとき、アクセルを踏むと後輪が空転して回転が上がる
    float sliding{std::fabs(std::sin(car.slipAngle()))};
    rpm += SPIN_RPM * sliding * throttle;
    return std::clamp(rpm, IDLE_RPM, REDLINE_RPM + 400.0f);
}

void DriftSound::update(const domain::Car& car, const DriftScore& drift, float throttle, float dt, bool paused)
{
    if (paused)
    {
        m_mixer.setLoop(Loop::Engine, 0.0f, m_rpm / SAMPLE_RPM);
        m_mixer.setLoop(Loop::Squeal, 0.0f, 1.0f);
        return;
    }

    // エンジン: アクセルを踏むほど大きく、回転が高いほど少し大きく
    m_rpm += (targetRpm(car, throttle) - m_rpm) * std::min(1.0f, dt * RPM_RESPONSE);
    float high{(m_rpm - IDLE_RPM) / (REDLINE_RPM - IDLE_RPM)};
    m_mixer.setLoop(Loop::Engine, 0.18f + 0.16f * throttle + 0.08f * high, m_rpm / SAMPLE_RPM);

    // スキール音: 深く滑るほど大きく、高く
    float slip{std::fabs(car.slipAngle())};
    float amount{smoothstep((slip - SQUEAL_START) / (SQUEAL_FULL - SQUEAL_START))};
    amount *= std::clamp((car.speed - 3.0f) / 10.0f, 0.0f, 1.0f);
    m_mixer.setLoop(Loop::Squeal, 0.55f * amount, 0.88f + 0.3f * amount);

    // ドリフトの連続が始まった
    if (drift.chaining() && !m_wasChaining)
    {
        m_mixer.play(Effect::DriftStart, 0.5f, 1.0f);
    }
    // 倍率が上がった。倍率が高いほど音を高くする（1 段で短 3 度）
    if (drift.chaining() && drift.multiplier() > m_lastMultiplier)
    {
        float rate{std::pow(2.0f, float(drift.multiplier() - 2) * 3.0f / 12.0f)};
        m_mixer.play(Effect::Combo, 0.6f, rate);
    }
    // 連続が終わった
    bool newResult{drift.sinceLastResult() < m_lastSinceResult && drift.lastResult() != DriftScore::Result::None};
    if (newResult)
    {
        if (drift.lastResult() == DriftScore::Result::Banked)
        {
            // 大きく稼いだほど大きく鳴らす
            float gain{0.45f + std::min(float(drift.lastPoints()) / 20000.0f, 0.4f)};
            m_mixer.play(Effect::Bank, gain, 1.0f);
        }
        else
        {
            m_mixer.play(Effect::Crash, 0.9f, 1.0f);
            m_crashCooldown = CRASH_COOLDOWN;
        }
    }
    // 連続していないときに壁に当たった
    m_crashCooldown = std::max(0.0f, m_crashCooldown - dt);
    if (car.hitWall && !m_wasHitWall && m_crashCooldown <= 0 && std::fabs(car.speed) > 5.0f)
    {
        m_mixer.play(Effect::Crash, 0.45f, 1.15f);
        m_crashCooldown = CRASH_COOLDOWN;
    }

    m_wasChaining = drift.chaining();
    m_lastMultiplier = drift.chaining() ? drift.multiplier() : 1;
    m_lastSinceResult = drift.sinceLastResult();
    m_wasHitWall = car.hitWall;
}

void DriftSound::updateWorld(const domain::StreetLamps& lamps, const domain::Props& props)
{
    // 数が減ったのはステージを変えたとき。鳴らさずに合わせる
    if (lamps.ignitions() > m_lastIgnitions)
    {
        // 灯ったばかりの街灯のうち、いちばん高い倍率の音で 1 回鳴らす（1 段で全音）
        int tier{1};
        for (const domain::StreetLamp& lamp : lamps.lamps())
        {
            if (lamp.lit && lamp.litAge < 0.1f)
            {
                tier = std::max(tier, lamp.tier);
            }
        }
        m_mixer.play(Effect::LampOn, 0.45f, std::pow(2.0f, float(tier - 1) * 2.0f / 12.0f));
    }
    m_lastIgnitions = lamps.ignitions();

    if (props.hits() > m_lastHits)
    {
        bool cone{props.lastHitKind() == domain::PropKind::Cone};
        float gain{std::clamp(props.lastHitSpeed() / 25.0f, 0.35f, 1.0f)};
        m_mixer.play(cone ? Effect::ConeHit : Effect::DrumHit, gain * (cone ? 0.7f : 0.8f), 0.92f + 0.16f * gain);
    }
    m_lastHits = props.hits();

    if (lamps.sinceAllLit() < m_lastSinceAllLit)
    {
        m_mixer.play(Effect::PortLit, 0.9f, 1.0f);
    }
    m_lastSinceAllLit = lamps.sinceAllLit();
}

}  // namespace adapters::audio
