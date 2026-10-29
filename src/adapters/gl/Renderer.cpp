#include "adapters/gl/Renderer.hpp"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#else
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>
#endif

namespace adapters::gl
{

using domain::dot;
using domain::Mat4;
using domain::normalize;
using domain::orthoTopLeft;
using domain::Vec3;

namespace
{

const char* VERSION =
#ifdef __EMSCRIPTEN__
    "#version 300 es\nprecision highp float;\n";
#else
    "#version 330 core\n";
#endif

const char* WORLD_VS = R"GLSL(
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;
uniform mat4 uViewProj;
uniform mat4 uModel;
out vec3 vNormal;
out vec3 vColor;
out vec3 vWorld;
void main() {
    vec4 world = uModel * vec4(aPos, 1.0);
    vWorld = world.xyz;
    vNormal = mat3(uModel) * aNormal;
    vColor = aColor;
    gl_Position = uViewProj * world;
}
)GLSL";

const char* WORLD_FS = R"GLSL(
in vec3 vNormal;
in vec3 vColor;
in vec3 vWorld;
uniform vec3 uCamPos;
out vec4 fragColor;
void main() {
    vec3 n = normalize(vNormal);
    if (!gl_FrontFacing) n = -n;
    vec3 sun = normalize(vec3(0.35, 0.86, 0.28));
    float light = 0.36 + 0.64 * max(dot(n, sun), 0.0);
    vec3 color = vColor * light;
    float dist = distance(vWorld, uCamPos);
    float fog = clamp((dist - 90.0) / 1500.0, 0.0, 1.0);
    vec3 haze = vec3(0.62, 0.74, 0.84);
    fragColor = vec4(mix(color, haze, fog), 1.0);
}
)GLSL";

const char* SKY_VS = R"GLSL(
out float vY;
void main() {
    vec2 pos = vec2((gl_VertexID == 2) ? 3.0 : -1.0, (gl_VertexID == 1) ? 3.0 : -1.0);
    vY = pos.y;
    gl_Position = vec4(pos, 1.0, 1.0);
}
)GLSL";

const char* SKY_FS = R"GLSL(
in float vY;
out vec4 fragColor;
void main() {
    float t = clamp(vY * 0.5 + 0.5, 0.0, 1.0);
    vec3 horizon = vec3(0.62, 0.74, 0.84);
    vec3 zenith = vec3(0.28, 0.48, 0.78);
    fragColor = vec4(mix(horizon, zenith, smoothstep(0.0, 1.0, t)), 1.0);
}
)GLSL";

const char* HUD_VS = R"GLSL(
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aColor;
uniform mat4 uOrtho;
out vec4 vColor;
void main() {
    vColor = aColor;
    gl_Position = uOrtho * vec4(aPos, 0.0, 1.0);
}
)GLSL";

const char* HUD_FS = R"GLSL(
in vec4 vColor;
out vec4 fragColor;
void main() {
    fragColor = vColor;
}
)GLSL";

const char* PARTICLE_VS = R"GLSL(
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUv;
layout(location = 2) in vec4 aColor;
uniform mat4 uViewProj;
out vec2 vUv;
out vec4 vColor;
out vec3 vWorld;
void main() {
    vUv = aUv;
    vColor = aColor;
    vWorld = aPos;
    gl_Position = uViewProj * vec4(aPos, 1.0);
}
)GLSL";

const char* PARTICLE_FS = R"GLSL(
in vec2 vUv;
in vec4 vColor;
in vec3 vWorld;
uniform vec3 uCamPos;
out vec4 fragColor;
void main() {
    // 中心が濃く、縁にいくほど薄い丸
    float edge = 1.0 - smoothstep(0.2, 1.0, length(vUv));
    float dist = distance(vWorld, uCamPos);
    float fog = clamp((dist - 90.0) / 1500.0, 0.0, 1.0);
    vec3 haze = vec3(0.62, 0.74, 0.84);
    fragColor = vec4(mix(vColor.rgb, haze, fog), vColor.a * edge);
}
)GLSL";

// 5x7。bit4 が左端。並びは 0-9 A-Z . : - / %
constexpr char GLYPH_ORDER[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ.:-/%";
const std::uint8_t FONT[][7] = {
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}, {0x0E, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0E},
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E},
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}, {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C},
    {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E},
    {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}, {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E},
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}, {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10},
    {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E}, {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
    {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}, {0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0E},
    {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}, {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F},
    {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}, {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11},
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10},
    {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}, {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11},
    {0x0E, 0x11, 0x10, 0x0E, 0x01, 0x11, 0x0E}, {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04},
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04},
    {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11}, {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11},
    {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}, {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C}, {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00},
    {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}, {0x01, 0x02, 0x04, 0x04, 0x08, 0x10, 0x10},
    {0x19, 0x1A, 0x02, 0x04, 0x08, 0x0B, 0x13},
};
static_assert(sizeof(FONT) / sizeof(FONT[0]) == sizeof(GLYPH_ORDER) - 1, "glyph count");

GLuint compileShader(GLenum type, const char* source)
{
    const char* parts[]{VERSION, source};
    GLuint shader{glCreateShader(type)};
    glShaderSource(shader, 2, parts, nullptr);
    glCompileShader(shader);
    GLint ok{0};
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        SDL_Log("shader compile failed: %s", log);
        return 0;
    }
    return shader;
}

GLuint linkProgram(const char* vs, const char* fs)
{
    GLuint vertex{compileShader(GL_VERTEX_SHADER, vs)};
    GLuint fragment{compileShader(GL_FRAGMENT_SHADER, fs)};
    if (!vertex || !fragment)
    {
        return 0;
    }
    GLuint program{glCreateProgram()};
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    GLint ok{0};
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        char log[1024];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        SDL_Log("shader link failed: %s", log);
        return 0;
    }
    return program;
}

int glyphIndex(char c)
{
    if (c >= 'a' && c <= 'z')
    {
        c = static_cast<char>(c - 'a' + 'A');
    }
    const char* found{std::strchr(GLYPH_ORDER, c)};
    if (!found)
    {
        return -1;
    }
    return static_cast<int>(found - GLYPH_ORDER);
}

}  // namespace

bool Renderer::init()
{
    glGenVertexArrays(1, &m_dummyVao);
    m_worldProgram = linkProgram(WORLD_VS, WORLD_FS);
    m_skyProgram = linkProgram(SKY_VS, SKY_FS);
    m_hudProgram = linkProgram(HUD_VS, HUD_FS);
    m_particleProgram = linkProgram(PARTICLE_VS, PARTICLE_FS);
    if (!m_worldProgram || !m_skyProgram || !m_hudProgram || !m_particleProgram)
    {
        return false;
    }
    m_uViewProj = glGetUniformLocation(m_worldProgram, "uViewProj");
    m_uModel = glGetUniformLocation(m_worldProgram, "uModel");
    m_uCamPos = glGetUniformLocation(m_worldProgram, "uCamPos");
    m_uOrtho = glGetUniformLocation(m_hudProgram, "uOrtho");
    m_uParticleViewProj = glGetUniformLocation(m_particleProgram, "uViewProj");
    m_uParticleCamPos = glGetUniformLocation(m_particleProgram, "uCamPos");

    // 板 1 枚は 4 頂点。頂点は xyz + uv + rgba
    glGenVertexArrays(1, &m_particleVao);
    glGenBuffers(1, &m_particleVbo);
    glGenBuffers(1, &m_particleIbo);
    glBindVertexArray(m_particleVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_particleVbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_particleIbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 9 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 9 * sizeof(float), reinterpret_cast<void*>(5 * sizeof(float)));

    glGenVertexArrays(1, &m_hudVao);
    glGenBuffers(1, &m_hudVbo);
    glGenBuffers(1, &m_hudIbo);
    glBindVertexArray(m_hudVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_hudVbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_hudIbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
    glBindVertexArray(m_dummyVao);

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    // lookAt は左右を直すために鏡映になっている。巻き方向が反転するので、表の判定も合わせる。
    glFrontFace(GL_CW);
    resize(m_width, m_height);
    return true;
}

void Renderer::clearFrame()
{
    glClearColor(0.62f, 0.74f, 0.84f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::resize(int newWidth, int newHeight)
{
    m_width = newWidth > 0 ? newWidth : 1;
    m_height = newHeight > 0 ? newHeight : 1;
    glViewport(0, 0, m_width, m_height);
}

void Renderer::drawSky()
{
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glUseProgram(m_skyProgram);
    glBindVertexArray(m_dummyVao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

void Renderer::beginWorld(const Mat4& viewProj, Vec3 cameraPos)
{
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glUseProgram(m_worldProgram);
    glUniformMatrix4fv(m_uViewProj, 1, GL_FALSE, viewProj.m);
    glUniform3f(m_uCamPos, cameraPos.x, cameraPos.y, cameraPos.z);
    m_viewProj = viewProj;
    m_cameraPos = cameraPos;
}

void Renderer::drawBillboards(std::vector<Billboard>& items, Vec3 cameraRight, Vec3 cameraUp)
{
    if (items.empty())
    {
        return;
    }
    // 半透明は奥から描かないと、手前の板が奥の板を隠してしまう
    Vec3 eye{m_cameraPos};
    std::sort(items.begin(), items.end(), [eye](const Billboard& a, const Billboard& b)
              { return dot(a.position - eye, a.position - eye) > dot(b.position - eye, b.position - eye); });

    m_particleVerts.clear();
    m_particleIndices.clear();
    const float corners[4][2]{{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
    for (const Billboard& item : items)
    {
        unsigned base{static_cast<unsigned>(m_particleVerts.size() / 9)};
        float half{item.size * 0.5f};
        for (const auto& corner : corners)
        {
            Vec3 p{item.position + cameraRight * (corner[0] * half) + cameraUp * (corner[1] * half)};
            float verts[]{p.x, p.y, p.z, corner[0], corner[1], item.r, item.g, item.b, item.a};
            m_particleVerts.insert(m_particleVerts.end(), verts, verts + 9);
        }
        unsigned idx[]{base, base + 1, base + 2, base, base + 2, base + 3};
        m_particleIndices.insert(m_particleIndices.end(), idx, idx + 6);
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glUseProgram(m_particleProgram);
    glUniformMatrix4fv(m_uParticleViewProj, 1, GL_FALSE, m_viewProj.m);
    glUniform3f(m_uParticleCamPos, m_cameraPos.x, m_cameraPos.y, m_cameraPos.z);
    glBindVertexArray(m_particleVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_particleVbo);
    glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(m_particleVerts.size() * sizeof(float)), m_particleVerts.data(),
                 GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_particleIbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, GLsizeiptr(m_particleIndices.size() * sizeof(unsigned)),
                 m_particleIndices.data(), GL_DYNAMIC_DRAW);
    glDrawElements(GL_TRIANGLES, GLsizei(m_particleIndices.size()), GL_UNSIGNED_INT, nullptr);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    // 次の draw のためにワールドのシェーダーへ戻す
    glUseProgram(m_worldProgram);
}

void Renderer::draw(const GpuMesh& mesh, const Mat4& model)
{
    if (mesh.indexCount <= 0)
    {
        return;
    }
    glBindVertexArray(mesh.vao);
    glUniformMatrix4fv(m_uModel, 1, GL_FALSE, model.m);
    glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, nullptr);
}

void Renderer::beginHud()
{
    m_hudVerts.clear();
    m_hudIndices.clear();
}

void Renderer::addQuad(float x0, float y0, float x1, float y1, float r, float g, float b, float a)
{
    unsigned base{static_cast<unsigned>(m_hudVerts.size() / 6)};
    float verts[] = {
        x0, y0, r, g, b, a, x1, y0, r, g, b, a, x1, y1, r, g, b, a, x0, y1, r, g, b, a,
    };
    m_hudVerts.insert(m_hudVerts.end(), verts, verts + 24);
    unsigned idx[]{base, base + 1, base + 2, base, base + 2, base + 3};
    m_hudIndices.insert(m_hudIndices.end(), idx, idx + 6);
}

void Renderer::drawLine(float x0, float y0, float x1, float y1, float thickness, float r, float g, float b, float a)
{
    float dx{x1 - x0};
    float dy{y1 - y0};
    float len{std::sqrt(dx * dx + dy * dy)};
    if (len < 0.001f)
    {
        return;
    }
    dx /= len;
    dy /= len;
    float px{-dy * thickness * 0.5f};
    float py{dx * thickness * 0.5f};
    unsigned base{static_cast<unsigned>(m_hudVerts.size() / 6)};
    float verts[] = {
        x0 + px, y0 + py, r, g, b, a, x0 - px, y0 - py, r, g, b, a,
        x1 - px, y1 - py, r, g, b, a, x1 + px, y1 + py, r, g, b, a,
    };
    m_hudVerts.insert(m_hudVerts.end(), verts, verts + 24);
    unsigned idx[]{base, base + 1, base + 2, base, base + 2, base + 3};
    m_hudIndices.insert(m_hudIndices.end(), idx, idx + 6);
}

void Renderer::drawText(float x, float y, float scale, const char* text, float r, float g, float b, float a)
{
    float pen{x};
    for (const char* cursor = text; *cursor; ++cursor)
    {
        char c{*cursor};
        if (c >= 'a' && c <= 'z')
        {
            c = static_cast<char>(c - 'a' + 'A');
        }
        if (c == ' ')
        {
            pen += 4.0f * scale;
            continue;
        }
        int index{glyphIndex(c)};
        if (index < 0)
        {
            pen += 6.0f * scale;
            continue;
        }
        const std::uint8_t* rows{FONT[index]};
        for (int row = 0; row < 7; ++row)
        {
            for (int col = 0; col < 5; ++col)
            {
                if ((rows[row] & (0x10 >> col)) == 0)
                {
                    continue;
                }
                float x0{pen + float(col) * scale};
                float y0{y + float(row) * scale};
                addQuad(x0, y0, x0 + scale, y0 + scale, r, g, b, a);
            }
        }
        pen += 6.0f * scale;
    }
}

void Renderer::flushHud()
{
    if (m_hudIndices.empty())
    {
        return;
    }
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(m_hudProgram);
    Mat4 ortho{orthoTopLeft(float(m_width), float(m_height))};
    glUniformMatrix4fv(m_uOrtho, 1, GL_FALSE, ortho.m);
    glBindVertexArray(m_hudVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_hudVbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_hudVerts.size() * sizeof(float)), m_hudVerts.data(),
                 GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_hudIbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_hudIndices.size() * sizeof(unsigned)),
                 m_hudIndices.data(), GL_DYNAMIC_DRAW);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_hudIndices.size()), GL_UNSIGNED_INT, nullptr);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

GpuMesh Renderer::upload(const CpuMesh& mesh)
{
    GpuMesh gpu;
    if (mesh.indices.empty())
    {
        return gpu;
    }
    glGenVertexArrays(1, &gpu.vao);
    glGenBuffers(1, &gpu.vbo);
    glGenBuffers(1, &gpu.ibo);
    glBindVertexArray(gpu.vao);
    glBindBuffer(GL_ARRAY_BUFFER, gpu.vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(mesh.vertices.size() * sizeof(float)), mesh.vertices.data(),
                 GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gpu.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(mesh.indices.size() * sizeof(std::uint32_t)),
                 mesh.indices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), reinterpret_cast<void*>(6 * sizeof(float)));
    gpu.indexCount = static_cast<int>(mesh.indices.size());
    glBindVertexArray(m_dummyVao);
    return gpu;
}

void Renderer::drawDecal(const GpuMesh& mesh)
{
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.0f, -4.0f);
    Mat4 identity;
    draw(mesh, identity);
    glDisable(GL_POLYGON_OFFSET_FILL);
}

void Renderer::update(GpuMesh& gpu, const CpuMesh& mesh)
{
    if (gpu.vao == 0)
    {
        if (!mesh.indices.empty())
        {
            gpu = upload(mesh);
        }
        return;
    }
    glBindVertexArray(gpu.vao);
    glBindBuffer(GL_ARRAY_BUFFER, gpu.vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(mesh.vertices.size() * sizeof(float)), mesh.vertices.data(),
                 GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gpu.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(mesh.indices.size() * sizeof(std::uint32_t)),
                 mesh.indices.data(), GL_DYNAMIC_DRAW);
    gpu.indexCount = static_cast<int>(mesh.indices.size());
    glBindVertexArray(m_dummyVao);
}

void Renderer::release(GpuMesh& gpu)
{
    if (gpu.vao != 0)
    {
        glDeleteVertexArrays(1, &gpu.vao);
        glDeleteBuffers(1, &gpu.vbo);
        glDeleteBuffers(1, &gpu.ibo);
    }
    gpu = GpuMesh{};
}

void Renderer::readPixels(std::vector<std::uint8_t>& rgba) const
{
    rgba.resize(std::size_t(m_width) * std::size_t(m_height) * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, m_width, m_height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
}

}  // namespace adapters::gl
