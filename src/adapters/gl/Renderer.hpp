#pragma once

#include "adapters/gl/CpuMesh.hpp"
#include "domain/Geom.hpp"

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
 * @brief OpenGL の描画。空・3D のワールド・2D の HUD を描く
 */
struct Renderer
{
    int width{1280};
    int height{720};

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
     * @brief メッシュを GPU に転送する
     * @param mesh 転送するメッシュ
     * @return 転送したメッシュ
     */
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
