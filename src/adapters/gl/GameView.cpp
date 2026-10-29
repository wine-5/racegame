#include "adapters/gl/GameView.hpp"

#include "adapters/gl/CarMesh.hpp"
#include "adapters/gl/Hud.hpp"
#include "adapters/gl/TrackMesh.hpp"

#include <cmath>

namespace adapters::gl
{

using domain::Car;
using domain::cross;
using domain::lookAt;
using domain::Mat4;
using domain::mul;
using domain::normalize;
using domain::perspective;
using domain::rotationX;
using domain::rotationY;
using domain::Track;
using domain::TrackPose;
using domain::translation;
using domain::Vec3;

namespace
{

constexpr float PI = 3.14159265f;

// 車の前後 2.4m の路面の高さから、車体の前後の傾きを求める
float carPitch(const Car& car, const Track& track)
{
    Vec3 forward{std::sin(car.yaw), 0, std::cos(car.yaw)};
    float preferS{car.ground.found ? car.ground.s : -1.0f};
    TrackPose ahead{track.query(car.x + forward.x * 2.4f, car.z + forward.z * 2.4f, car.y, preferS)};
    TrackPose behind{track.query(car.x - forward.x * 2.4f, car.z - forward.z * 2.4f, car.y, preferS)};
    if (!ahead.found || !behind.found)
    {
        return 0;
    }
    return std::atan2(ahead.centerY - behind.centerY, 4.8f);
}

}  // namespace

bool GameView::init(const Track& track)
{
    if (!m_renderer.init())
    {
        return false;
    }
    CpuMesh course;
    CpuMesh body;
    CpuMesh wheel;
    CpuMesh shadow;
    buildTrackMesh(track, course);
    buildCarBody(body);
    buildCarWheel(wheel);
    buildCarShadow(shadow);
    m_course = m_renderer.upload(course);
    m_body = m_renderer.upload(body);
    m_wheel = m_renderer.upload(wheel);
    m_shadow = m_renderer.upload(shadow);
    return true;
}

void GameView::render(const application::GameSession& session, float dt, int width, int height)
{
    m_camera.update(session.car(), session.track(), dt);
    m_smoke.update(session.car(), dt, !session.paused());
    if (m_skidMarks.update(session.car(), !session.paused()) || m_skidMarksDirty)
    {
        m_skidMarks.build(m_skidMesh);
        m_renderer.update(m_skidGpu, m_skidMesh);
        m_skidMarksDirty = false;
    }

    m_renderer.resize(width, height);
    m_renderer.clearFrame();
    m_renderer.drawSky();

    float aspect{float(width) / float(height > 0 ? height : 1)};
    Mat4 proj{perspective(m_camera.fovDegrees() * PI / 180.0f, aspect, 0.15f, 4000.0f)};
    Mat4 view{lookAt(m_camera.eye(), m_camera.look(), {0, 1, 0})};
    m_renderer.beginWorld(mul(proj, view), m_camera.eye());

    Mat4 identity;
    m_renderer.draw(m_course, identity);
    m_renderer.drawDecal(m_skidGpu);
    drawCar(session.car(), session.track());

    // 半透明のものは不透明なものをすべて描いてから
    Vec3 viewDir{normalize(m_camera.look() - m_camera.eye())};
    Vec3 cameraRight{normalize(cross(viewDir, Vec3{0, 1, 0}))};
    Vec3 cameraUp{cross(cameraRight, viewDir)};
    m_billboards.clear();
    m_smoke.collect(m_billboards, m_camera.eye());
    m_renderer.drawBillboards(m_billboards, cameraRight, cameraUp);

    drawHud(m_renderer, session, m_camera, m_showHelp);
}

void GameView::drawCar(const Car& car, const Track& track)
{
    Mat4 shadow{mul(translation(car.x, car.y + 0.08f, car.z), rotationY(car.yaw))};
    m_renderer.draw(m_shadow, shadow);

    float pitch{carPitch(car, track)};
    float roll{std::atan(car.ground.cross)};
    Mat4 model{carModelMatrix(car, pitch, roll)};
    m_renderer.draw(m_body, model);

    const float wheelX{0.82f};
    const float wheelZ[2]{1.42f, -1.38f};
    const float steerForWheel[2]{car.steerAngle, 0};
    for (int i{}; i < 2; ++i)
    {
        for (int side{-1}; side <= 1; side += 2)
        {
            Mat4 place{mul(translation(float(side) * wheelX, 0.34f, wheelZ[i]),
                           mul(rotationY(steerForWheel[i]), rotationX(car.wheelSpin)))};
            m_renderer.draw(m_wheel, mul(model, place));
        }
    }
}

void GameView::readPixels(std::vector<std::uint8_t>& rgba, int& width, int& height) const
{
    m_renderer.readPixels(rgba);
    width = m_renderer.width();
    height = m_renderer.height();
}

}  // namespace adapters::gl
