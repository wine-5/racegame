#pragma once

#include "domain/Geom.hpp"

namespace adapters::gl
{

/**
 * @brief 画面全体にかける効果の強さ
 */
struct PostEffects
{
    float bloom{0.9f};           // 光るもののにじみの強さ
    float bloomThreshold{0.7f};  // この明るさを超えた所がにじむ（0..1）
    float speedBlur{};           // 速さで画面の端が中心へ流れるぶれ（0..1）
    float aberration{};          // 画面の端で色がずれる量（0..1）。深いドリフトで強める
    float vignette{0.35f};       // 画面の四隅を暗くする量（0..1）
    float saturation{1.0f};      // 彩度（1 で元のまま）
    float flash{};               // 画面全体を flashColor へ寄せる量（0..1）
    domain::Vec3 flashColor{1.0f, 1.0f, 1.0f};
    float time{};  // 粒子感（フィルムグレイン）を動かすための時間（秒）
};

/**
 * @brief 3D の画面を画面外の画像に描き、ブルームなどの効果をかけて画面に出す
 * @details begin のあと 3D を描き、finish で効果をかけて画面へ出す。HUD はそのあとに描くと効果がかからない。
 *          ブルームは明るい所を取り出し、半分と 4 分の 1 の大きさでぼかして元の画面に足す。
 */
class PostProcess
{
public:
    /**
     * @brief シェーダーと四角い板を作る
     * @return 失敗したら false
     */
    bool init();

    /**
     * @brief これから描く 3D を画面外の画像へ向ける
     * @details 大きさが変わったら画像を作り直す。
     * @param width 画面の幅（ピクセル）
     * @param height 画面の高さ（ピクセル）
     */
    void begin(int width, int height);

    /**
     * @brief 効果をかけて画面へ出す
     * @param effects 効果の強さ
     */
    void finish(const PostEffects& effects);

private:
    struct Target
    {
        unsigned framebuffer{};
        unsigned texture{};
        int width{};
        int height{};
    };

    /// 画像と、そこへ描くためのフレームバッファを作る
    void createTarget(Target& target, int width, int height, bool depth);

    /// 画像とフレームバッファを消す
    void destroyTarget(Target& target);

    /// 画面いっぱいの三角形で、source を target へ描く
    void pass(unsigned program, const Target& target, unsigned source);

    unsigned m_brightProgram{};
    unsigned m_blurProgram{};
    unsigned m_compositeProgram{};
    unsigned m_vao{};
    unsigned m_depth{};
    Target m_scene;
    Target m_half[2];
    Target m_quarter[2];
    int m_width{};
    int m_height{};
};

}  // namespace adapters::gl
