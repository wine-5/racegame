#include "adapters/gl/PostProcess.hpp"

#include "adapters/gl/GlApi.hpp"
#include "adapters/gl/Shader.hpp"

namespace adapters::gl
{

namespace
{

const char* FULLSCREEN_VS = R"GLSL(
out vec2 vUv;
void main() {
    vec2 pos = vec2((gl_VertexID == 2) ? 3.0 : -1.0, (gl_VertexID == 1) ? 3.0 : -1.0);
    vUv = pos * 0.5 + 0.5;
    gl_Position = vec4(pos, 0.0, 1.0);
}
)GLSL";

// 明るい所だけを取り出す。4 点を平均して、ちらつきを抑える
const char* BRIGHT_FS = R"GLSL(
in vec2 vUv;
uniform sampler2D uImage;
uniform vec2 uTexel;
uniform float uThreshold;
out vec4 fragColor;
void main() {
    vec3 c = texture(uImage, vUv + uTexel * vec2(-0.5, -0.5)).rgb;
    c += texture(uImage, vUv + uTexel * vec2(0.5, -0.5)).rgb;
    c += texture(uImage, vUv + uTexel * vec2(-0.5, 0.5)).rgb;
    c += texture(uImage, vUv + uTexel * vec2(0.5, 0.5)).rgb;
    c *= 0.25;
    float level = max(c.r, max(c.g, c.b));
    float keep = smoothstep(uThreshold, uThreshold + 0.25, level);
    fragColor = vec4(c * keep, 1.0);
}
)GLSL";

// 9 点のガウスぼかし。uDirection の向きにだけぼかす（縦と横で 2 回かける）
const char* BLUR_FS = R"GLSL(
in vec2 vUv;
uniform sampler2D uImage;
uniform vec2 uTexel;
uniform vec2 uDirection;
out vec4 fragColor;
void main() {
    vec2 step = uTexel * uDirection;
    vec3 c = texture(uImage, vUv).rgb * 0.227027;
    c += (texture(uImage, vUv + step * 1.3846).rgb + texture(uImage, vUv - step * 1.3846).rgb) * 0.3162162;
    c += (texture(uImage, vUv + step * 3.2308).rgb + texture(uImage, vUv - step * 3.2308).rgb) * 0.0702703;
    fragColor = vec4(c, 1.0);
}
)GLSL";

const char* COMPOSITE_FS = R"GLSL(
in vec2 vUv;
uniform sampler2D uScene;
uniform sampler2D uBloomHalf;
uniform sampler2D uBloomQuarter;
uniform float uBloom;
uniform float uSpeedBlur;
uniform float uAberration;
uniform float uVignette;
uniform float uSaturation;
uniform float uFlash;
uniform vec3 uFlashColor;
uniform float uTime;
out vec4 fragColor;

// 画面の端ほど、赤と青を外と内へずらす
vec3 sceneAt(vec2 uv) {
    vec2 fromCenter = uv - 0.5;
    float shift = uAberration * 0.012 * dot(fromCenter, fromCenter) * 4.0;
    return vec3(texture(uScene, uv - fromCenter * shift).r,
                texture(uScene, uv).g,
                texture(uScene, uv + fromCenter * shift).b);
}

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    vec3 color = sceneAt(vUv);
    // 速いほど、画面の端が中心から外へ流れる（中心はくっきりのまま）
    if (uSpeedBlur > 0.001) {
        vec2 fromCenter = vUv - 0.5;
        float amount = uSpeedBlur * 0.07 * smoothstep(0.12, 0.7, length(fromCenter));
        for (int i = 1; i <= 6; ++i) {
            color += sceneAt(vUv - fromCenter * amount * float(i) / 6.0);
        }
        color /= 7.0;
    }
    color += (texture(uBloomHalf, vUv).rgb * 0.7 + texture(uBloomQuarter, vUv).rgb * 1.1) * uBloom;
    float luma = dot(color, vec3(0.299, 0.587, 0.114));
    color = mix(vec3(luma), color, uSaturation);
    float edge = smoothstep(0.85, 0.3, length(vUv - 0.5));
    color *= mix(1.0 - uVignette, 1.0, edge);
    color = mix(color, uFlashColor, uFlash);
    color += (hash(vUv * 731.0 + uTime) - 0.5) * 0.03;
    fragColor = vec4(color, 1.0);
}
)GLSL";

void setTexel(unsigned program, int width, int height)
{
    glUniform2f(glGetUniformLocation(program, "uTexel"), 1.0f / float(width), 1.0f / float(height));
}

}  // namespace

bool PostProcess::init()
{
    m_brightProgram = buildProgram(FULLSCREEN_VS, BRIGHT_FS);
    m_blurProgram = buildProgram(FULLSCREEN_VS, BLUR_FS);
    m_compositeProgram = buildProgram(FULLSCREEN_VS, COMPOSITE_FS);
    glGenVertexArrays(1, &m_vao);
    return m_brightProgram && m_blurProgram && m_compositeProgram;
}

void PostProcess::createTarget(Target& target, int width, int height, bool depth)
{
    target.width = width > 0 ? width : 1;
    target.height = height > 0 ? height : 1;
    glGenTextures(1, &target.texture);
    glBindTexture(GL_TEXTURE_2D, target.texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, target.width, target.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &target.framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target.texture, 0);
    if (depth)
    {
        glGenRenderbuffers(1, &m_depth);
        glBindRenderbuffer(GL_RENDERBUFFER, m_depth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, target.width, target.height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depth);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void PostProcess::destroyTarget(Target& target)
{
    if (target.framebuffer)
    {
        glDeleteFramebuffers(1, &target.framebuffer);
        glDeleteTextures(1, &target.texture);
    }
    target = Target{};
}

void PostProcess::begin(int width, int height)
{
    if (width != m_width || height != m_height)
    {
        destroyTarget(m_scene);
        for (Target& target : m_half)
        {
            destroyTarget(target);
        }
        for (Target& target : m_quarter)
        {
            destroyTarget(target);
        }
        if (m_depth)
        {
            glDeleteRenderbuffers(1, &m_depth);
            m_depth = 0;
        }
        m_width = width;
        m_height = height;
        createTarget(m_scene, width, height, true);
        for (Target& target : m_half)
        {
            createTarget(target, width / 2, height / 2, false);
        }
        for (Target& target : m_quarter)
        {
            createTarget(target, width / 4, height / 4, false);
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, m_scene.framebuffer);
    glViewport(0, 0, m_width, m_height);
}

void PostProcess::pass(unsigned program, const Target& target, unsigned source)
{
    glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
    glViewport(0, 0, target.width, target.height);
    glUseProgram(program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, source);
    glUniform1i(glGetUniformLocation(program, "uImage"), 0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void PostProcess::finish(const PostEffects& effects)
{
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glBindVertexArray(m_vao);

    // 明るい所を半分の大きさへ取り出し、縦横にぼかす
    glUseProgram(m_brightProgram);
    setTexel(m_brightProgram, m_scene.width, m_scene.height);
    glUniform1f(glGetUniformLocation(m_brightProgram, "uThreshold"), effects.bloomThreshold);
    pass(m_brightProgram, m_half[0], m_scene.texture);
    glUseProgram(m_blurProgram);
    setTexel(m_blurProgram, m_half[0].width, m_half[0].height);
    glUniform2f(glGetUniformLocation(m_blurProgram, "uDirection"), 1.0f, 0.0f);
    pass(m_blurProgram, m_half[1], m_half[0].texture);
    glUniform2f(glGetUniformLocation(m_blurProgram, "uDirection"), 0.0f, 1.0f);
    pass(m_blurProgram, m_half[0], m_half[1].texture);

    // さらに 4 分の 1 へ縮めて、広くにじませる
    glUseProgram(m_blurProgram);
    setTexel(m_blurProgram, m_quarter[0].width, m_quarter[0].height);
    glUniform2f(glGetUniformLocation(m_blurProgram, "uDirection"), 1.0f, 0.0f);
    pass(m_blurProgram, m_quarter[1], m_half[0].texture);
    glUniform2f(glGetUniformLocation(m_blurProgram, "uDirection"), 0.0f, 1.0f);
    pass(m_blurProgram, m_quarter[0], m_quarter[1].texture);

    // 画面へ重ねて出す
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, m_width, m_height);
    unsigned program{m_compositeProgram};
    glUseProgram(program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_scene.texture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_half[0].texture);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_quarter[0].texture);
    glUniform1i(glGetUniformLocation(program, "uScene"), 0);
    glUniform1i(glGetUniformLocation(program, "uBloomHalf"), 1);
    glUniform1i(glGetUniformLocation(program, "uBloomQuarter"), 2);
    glUniform1f(glGetUniformLocation(program, "uBloom"), effects.bloom);
    glUniform1f(glGetUniformLocation(program, "uSpeedBlur"), effects.speedBlur);
    glUniform1f(glGetUniformLocation(program, "uAberration"), effects.aberration);
    glUniform1f(glGetUniformLocation(program, "uVignette"), effects.vignette);
    glUniform1f(glGetUniformLocation(program, "uSaturation"), effects.saturation);
    glUniform1f(glGetUniformLocation(program, "uFlash"), effects.flash);
    glUniform3f(glGetUniformLocation(program, "uFlashColor"), effects.flashColor.x, effects.flashColor.y,
                effects.flashColor.z);
    glUniform1f(glGetUniformLocation(program, "uTime"), effects.time);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_DEPTH_TEST);
}

}  // namespace adapters::gl
