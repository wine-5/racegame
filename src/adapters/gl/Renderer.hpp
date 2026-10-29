#pragma once

#include "adapters/gl/CpuMesh.hpp"
#include "domain/Geom.hpp"

#include <vector>

namespace adapters::gl
{

struct GpuMesh
{
    unsigned vao{};
    unsigned vbo{};
    unsigned ibo{};
    int indexCount{};
};

struct Renderer
{
    int width{1280};
    int height{720};

    bool init();
    void resize(int width, int height);
    void clearFrame();
    void drawSky();
    void beginWorld(const domain::Mat4& viewProj, domain::Vec3 cameraPos);
    void draw(const GpuMesh& mesh, const domain::Mat4& model);
    void beginHud();
    void addQuad(float x0, float y0, float x1, float y1, float r, float g, float b, float a);
    void drawLine(float x0, float y0, float x1, float y1, float thickness, float r, float g, float b, float a);
    void drawText(float x, float y, float scale, const char* text, float r, float g, float b, float a);
    void flushHud();
    GpuMesh upload(const CpuMesh& mesh);

    unsigned worldProgram{};
    unsigned skyProgram{};
    unsigned hudProgram{};
    unsigned dummyVao{};
    unsigned hudVao{};
    unsigned hudVbo{};
    unsigned hudIbo{};
    int uViewProj{-1};
    int uModel{-1};
    int uCamPos{-1};
    int uOrtho{-1};
    std::vector<float> hudVerts;
    std::vector<unsigned> hudIndices;
};

}  // namespace adapters::gl
