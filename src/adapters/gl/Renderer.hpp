#pragma once

#include "adapters/gl/CpuMesh.hpp"
#include "domain/Geom.hpp"

#include <cstdint>
#include <vector>

namespace adapters::gl
{

/**
 * @brief GPU に転送済みのメッシュ
 */
struct GpuMesh
{
    unsigned vao{};
    unsigned vbo{};
    unsigned ibo{};
    int indexCount{};
};

/**
 * @brief カメラの方を向く半透明の板（煙などのパーティクル）
 */
struct Billboard
{
    domain::Vec3 position;
    float size{1.0f};  // 一辺の長さ（m）
    float r{1.0f};
    float g{1.0f};
    float b{1.0f};
    float a{1.0f};  // 不透明度
};

/**
 * @brief OpenGL の描画。空・3D のワールド・2D の HUD を描く
 */
class Renderer
{
public:
    /**
     * @brief シェーダーとバッファを作る
     * @details GL コンテキストを作ったあとに呼ぶ。
     * @return 失敗したら false
     */
    bool init();

    /**
     * @brief 描画先の大きさを変える
     * @param width 幅（ピクセル）
     * @param height 高さ（ピクセル）
     */
    void resize(int width, int height);

    /// 画面を消す
    void clearFrame();

    /// 空を描く
    void drawSky();

    /**
     * @brief 3D のワールドの描画を始める
     * @param viewProj 投影行列 * ビュー行列
     * @param cameraPos カメラの位置（霧の計算に使う）
     */
    void beginWorld(const domain::Mat4& viewProj, domain::Vec3 cameraPos);

    /**
     * @brief メッシュを描く
     * @param mesh 描くメッシュ
     * @param model モデル行列
     */
    void draw(const GpuMesh& mesh, const domain::Mat4& model);

    /**
     * @brief 半透明の板をまとめて描く
     * @details beginWorld のあと、不透明なものをすべて描いてから呼ぶ。奥から順に並べ替えて描く。
     *          板は丸くぼかして描くので、煙や埃に使える。
     * @param items 描く板。並べ替えるので書き換わる
     * @param cameraRight カメラの右方向
     * @param cameraUp カメラの上方向
     */
    void drawBillboards(std::vector<Billboard>& items, domain::Vec3 cameraRight, domain::Vec3 cameraUp);

    /// HUD の描画を始める。以降の addQuad / drawLine / drawText は flushHud でまとめて描く
    void beginHud();

    /**
     * @brief HUD に四角を追加する
     * @param x0 左
     * @param y0 上
     * @param x1 右
     * @param y1 下
     * @param r 赤
     * @param g 緑
     * @param b 青
     * @param a 不透明度
     */
    void addQuad(float x0, float y0, float x1, float y1, float r, float g, float b, float a);

    /**
     * @brief HUD に線を追加する
     * @param x0 始点の x
     * @param y0 始点の y
     * @param x1 終点の x
     * @param y1 終点の y
     * @param thickness 太さ（ピクセル）
     * @param r 赤
     * @param g 緑
     * @param b 青
     * @param a 不透明度
     */
    void drawLine(float x0, float y0, float x1, float y1, float thickness, float r, float g, float b, float a);

    /**
     * @brief HUD に文字列を追加する
     * @details 内蔵のビットマップフォント。英大文字・数字・一部の記号だけ描ける。
     * @param x 左上の x
     * @param y 左上の y
     * @param scale 文字の倍率
     * @param text 文字列
     * @param r 赤
     * @param g 緑
     * @param b 青
     * @param a 不透明度
     */
    void drawText(float x, float y, float scale, const char* text, float r, float g, float b, float a);

    /// たまった HUD をまとめて描く
    void flushHud();

    /**
     * @brief 路面に貼るメッシュ（タイヤ痕など）を描く
     * @details 路面と同じ高さでもちらつかないよう、少しだけ手前にずらして描く。
     * @param mesh 描くメッシュ
     */
    void drawDecal(const GpuMesh& mesh);

    /**
     * @brief 転送済みのメッシュの中身を入れ替える
     * @details 毎フレーム形が変わるメッシュに使う。まだ転送していなければ新しく作る。
     * @param gpu 入れ替える先
     * @param mesh 新しい中身
     */
    void update(GpuMesh& gpu, const CpuMesh& mesh);

    /**
     * @brief メッシュを GPU に転送する
     * @param mesh 転送するメッシュ
     * @return 転送したメッシュ
     */
    GpuMesh upload(const CpuMesh& mesh);

    /**
     * @brief 描画した画面を読み出す
     * @details 下の行から順に並ぶ（OpenGL の向き）。
     * @param rgba 読み出し先。幅 × 高さ × 4 バイトになる
     */
    void readPixels(std::vector<std::uint8_t>& rgba) const;

    /// 描画先の幅（ピクセル）
    int width() const { return m_width; }

    /// 描画先の高さ（ピクセル）
    int height() const { return m_height; }

private:
    int m_width{1280};
    int m_height{720};
    unsigned m_worldProgram{};
    unsigned m_skyProgram{};
    unsigned m_hudProgram{};
    unsigned m_dummyVao{};
    unsigned m_hudVao{};
    unsigned m_hudVbo{};
    unsigned m_hudIbo{};
    int m_uViewProj{-1};
    int m_uModel{-1};
    int m_uCamPos{-1};
    int m_uOrtho{-1};
    unsigned m_particleProgram{};
    unsigned m_particleVao{};
    unsigned m_particleVbo{};
    unsigned m_particleIbo{};
    int m_uParticleViewProj{-1};
    int m_uParticleCamPos{-1};
    domain::Mat4 m_viewProj;
    domain::Vec3 m_cameraPos;
    std::vector<float> m_particleVerts;
    std::vector<unsigned> m_particleIndices;
    std::vector<float> m_hudVerts;
    std::vector<unsigned> m_hudIndices;
};

}  // namespace adapters::gl
