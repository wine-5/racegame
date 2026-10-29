#pragma once

#include <cmath>

namespace domain
{

/**
 * @brief 3 次元ベクトル
 * @details 右手系。x が東、y が上、z が北。yaw 0 は +z を向き、正の yaw は右（時計回り）。
 */

struct Vec3
{
    float x{};
    float y{};
    float z{};
};

/// ベクトルの和
inline Vec3 operator+(Vec3 a, Vec3 b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}
/// ベクトルの差
inline Vec3 operator-(Vec3 a, Vec3 b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}
/// ベクトルのスカラー倍
inline Vec3 operator*(Vec3 a, float s)
{
    return {a.x * s, a.y * s, a.z * s};
}
/// ベクトルのスカラー倍
inline Vec3 operator*(float s, Vec3 a)
{
    return a * s;
}

/// 内積
inline float dot(Vec3 a, Vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

/// 外積
inline Vec3 cross(Vec3 a, Vec3 b)
{
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

/// ベクトルの長さ
inline float length(Vec3 a)
{
    return std::sqrt(dot(a, a));
}

/**
 * @brief 長さ 1 のベクトルにする
 * @param a 元のベクトル
 * @return 正規化したベクトル。長さがほぼ 0 のときは a をそのまま返す
 */
inline Vec3 normalize(Vec3 a)
{
    float len = length(a);
    if (len < 1e-6f)
    {
        return {0, 1, 0};
    }
    return a * (1.0f / len);
}

/**
 * @brief 値を範囲内に収める
 * @param v 値
 * @param lo 下限
 * @param hi 上限
 * @return lo 以上 hi 以下に収めた値
 */
inline float clampf(float v, float lo, float hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

/**
 * @brief 角度を -π..π に収める
 * @param a 角度（ラジアン）
 * @return -π..π に折り返した角度
 */
inline float wrapPi(float a)
{
    const float pi = 3.14159265f;
    while (a > pi)
        a -= pi * 2.0f;
    while (a < -pi)
        a += pi * 2.0f;
    return a;
}

/**
 * @brief 4x4 行列
 * @details 列優先。OpenGL にそのまま渡せる。既定値は単位行列。
 */
struct Mat4
{
    float m[16]{
        1, 0, 0, 0,  //
        0, 1, 0, 0,  //
        0, 0, 1, 0,  //
        0, 0, 0, 1,  //
    };
};

/**
 * @brief 行列の積
 * @param a 左の行列
 * @param b 右の行列
 * @return a * b
 */
inline Mat4 mul(const Mat4& a, const Mat4& b)
{
    Mat4 r;
    for (int c = 0; c < 4; ++c)
    {
        for (int row = 0; row < 4; ++row)
        {
            r.m[c * 4 + row] = a.m[0 * 4 + row] * b.m[c * 4 + 0] + a.m[1 * 4 + row] * b.m[c * 4 + 1] +
                               a.m[2 * 4 + row] * b.m[c * 4 + 2] + a.m[3 * 4 + row] * b.m[c * 4 + 3];
        }
    }
    return r;
}

/**
 * @brief 平行移動の行列を作る
 * @param x x 方向の移動量
 * @param y y 方向の移動量
 * @param z z 方向の移動量
 * @return 平行移動の行列
 */
inline Mat4 translation(float x, float y, float z)
{
    Mat4 r;
    r.m[12] = x;
    r.m[13] = y;
    r.m[14] = z;
    return r;
}

/**
 * @brief x 軸まわりの回転行列を作る
 * @param angle 回転角（ラジアン）
 * @return 回転行列
 */
inline Mat4 rotationX(float angle)
{
    float c = std::cos(angle);
    float s = std::sin(angle);
    Mat4 r;
    r.m[5] = c;
    r.m[6] = s;
    r.m[9] = -s;
    r.m[10] = c;
    return r;
}

/**
 * @brief y 軸まわりの回転行列を作る
 * @param yaw 回転角（ラジアン、右が正）
 * @return 回転行列
 */
inline Mat4 rotationY(float yaw)
{
    float c = std::cos(yaw);
    float s = std::sin(yaw);
    Mat4 r;
    // 列0 = ローカル +X（右）、列2 = ローカル +Z（前）
    r.m[0] = c;
    r.m[2] = -s;
    r.m[8] = s;
    r.m[10] = c;
    return r;
}

/**
 * @brief 透視投影の行列を作る
 * @param fovYRadians 縦方向の視野角（ラジアン）
 * @param aspect 横 / 縦の比
 * @param nearZ 手前のクリップ面までの距離
 * @param farZ 奥のクリップ面までの距離
 * @return 投影行列
 */
inline Mat4 perspective(float fovYRadians, float aspect, float nearZ, float farZ)
{
    float f = 1.0f / std::tan(fovYRadians * 0.5f);
    Mat4 r;
    for (int i = 0; i < 16; ++i)
        r.m[i] = 0;
    r.m[0] = f / aspect;
    r.m[5] = f;
    r.m[10] = (farZ + nearZ) / (nearZ - farZ);
    r.m[11] = -1.0f;
    r.m[14] = (2.0f * farZ * nearZ) / (nearZ - farZ);
    return r;
}

/**
 * @brief 視点から注視点を見るビュー行列を作る
 * @param eye 視点
 * @param target 注視点
 * @param up 上方向
 * @return ビュー行列
 */
inline Mat4 lookAt(Vec3 eye, Vec3 target, Vec3 up)
{
    Vec3 f = normalize(target - eye);
    // cross(f, up) だと、前が +z のとき画面の右が車の左になる。
    // 上方向は cross(s, f) のままなので、左右だけを直す。
    Vec3 s = normalize(cross(up, f));
    Vec3 u = cross(f, s);
    Mat4 r;
    r.m[0] = s.x;
    r.m[1] = u.x;
    r.m[2] = -f.x;
    r.m[4] = s.y;
    r.m[5] = u.y;
    r.m[6] = -f.y;
    r.m[8] = s.z;
    r.m[9] = u.z;
    r.m[10] = -f.z;
    r.m[12] = -dot(s, eye);
    r.m[13] = -dot(u, eye);
    r.m[14] = dot(f, eye);
    return r;
}

/**
 * @brief 画面ピクセル座標の正射影行列を作る
 * @details 原点は左上、y は下向き。
 * @param width 画面の幅（ピクセル）
 * @param height 画面の高さ（ピクセル）
 * @return 正射影行列
 */
inline Mat4 orthoTopLeft(float width, float height)
{
    Mat4 r;
    for (int i = 0; i < 16; ++i)
        r.m[i] = 0;
    r.m[0] = 2.0f / width;
    r.m[5] = -2.0f / height;
    r.m[10] = -1.0f;
    r.m[12] = -1.0f;
    r.m[13] = 1.0f;
    r.m[15] = 1.0f;
    return r;
}

/**
 * @brief 位置と 3 つの軸から姿勢の行列を作る
 * @details 列は right, up, forward。ローカル +Z が前、+X が右、+Y が上。
 * @param pos 位置
 * @param right 右方向
 * @param up 上方向
 * @param forward 前方向
 * @return 姿勢の行列
 */
inline Mat4 basis(Vec3 pos, Vec3 right, Vec3 up, Vec3 forward)
{
    Mat4 r;
    r.m[0] = right.x;
    r.m[1] = right.y;
    r.m[2] = right.z;
    r.m[4] = up.x;
    r.m[5] = up.y;
    r.m[6] = up.z;
    r.m[8] = forward.x;
    r.m[9] = forward.y;
    r.m[10] = forward.z;
    r.m[12] = pos.x;
    r.m[13] = pos.y;
    r.m[14] = pos.z;
    return r;
}

}  // namespace domain
