#include "adapters/gl/GameView.hpp"

#include "adapters/gl/CarMesh.hpp"
#include "adapters/gl/Hud.hpp"
#include "adapters/gl/PortScenery.hpp"
#include "adapters/gl/TrackMesh.hpp"

#include <algorithm>
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

bool GameView::init()
{
    if (!m_renderer.init())
    {
        return false;
    }
    CpuMesh body;
    CpuMesh wheel;
    CpuMesh shadow;
    buildCarBody(body);
    buildCarWheel(wheel);
    buildCarShadow(shadow);
    m_body = m_renderer.upload(body);
    m_wheel = m_renderer.upload(wheel);
    m_shadow = m_renderer.upload(shadow);
    return true;
}

void GameView::loadStage(const Track& track, Scenery scenery)
{
    m_scenery = scenery;
    CpuMesh course;
    if (scenery == Scenery::Circuit)
    {
        buildTrackMesh(track, course);
        m_lamps.clear();
        m_renderer.setLighting(Lighting{});
        m_skidMarks.setRoadColor({0.17f, 0.17f, 0.19f});
    }
    else
    {
        buildPortScenery(track, course, m_lamps);
        m_renderer.setLighting(portLighting());
        m_skidMarks.setRoadColor(portRoadColor());
    }
    m_renderer.release(m_course);
    m_course = m_renderer.upload(course);
    snapCamera();
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

    chooseLights(session.car());
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

    drawHud(m_renderer, session, m_camera, m_showHelp, m_scenery == Scenery::Circuit);
}

void GameView::chooseLights(const Car& car)
{
    m_lights.clear();
    if (m_scenery != Scenery::Circuit)
    {
        // 夜は車のライトを点ける。前は道を照らし、後ろは煙を赤く染める
        Vec3 forward{std::sin(car.yaw), 0, std::cos(car.yaw)};
        Vec3 position{car.x, car.y, car.z};
        m_lights.push_back({position + forward * 7.0f + Vec3{0, 0.9f, 0}, Vec3{0.95f, 0.9f, 0.75f} * 1.1f, 17.0f});
        m_lights.push_back({position - forward * 2.8f + Vec3{0, 0.7f, 0}, Vec3{1.0f, 0.08f, 0.05f} * 0.9f, 7.0f});
    }
    // 光の数には上限があるので、カメラに近い街灯から使う
    Vec3 eye{m_camera.eye()};
    std::size_t room{std::size_t(MAX_POINT_LIGHTS) - m_lights.size()};
    std::size_t count{std::min(room, m_lamps.size())};
    std::partial_sort(m_lamps.begin(), m_lamps.begin() + std::ptrdiff_t(count), m_lamps.end(),
                      [eye](const PointLight& a, const PointLight& b)
                      {
                          Vec3 da{a.position - eye};
                          Vec3 db{b.position - eye};
                          // 遠くまで届く光は、遠くにあっても先に選ぶ
                          return domain::dot(da, da) / (a.radius * a.radius) <
                                 domain::dot(db, db) / (b.radius * b.radius);
                      });
    m_lights.insert(m_lights.end(), m_lamps.begin(), m_lamps.begin() + std::ptrdiff_t(count));
    m_renderer.setPointLights(m_lights);
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
