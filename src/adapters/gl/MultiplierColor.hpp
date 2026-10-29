#pragma once

#include "domain/Geom.hpp"

namespace adapters::gl
{

/**
 * @brief ドリフトの倍率ごとの色
 * @details 火花・街灯など、倍率を見せる演出で同じ色を使う。X1 緑、X2 青、X3 マゼンタ、X4 金、X5 白。
 * @param multiplier 倍率（1..5）
 * @return 色（0..1）
 */
inline domain::Vec3 multiplierColor(int multiplier)
{
    switch (multiplier)
    {
        case 1:
            return {0.25f, 1.0f, 0.65f};
        case 2:
            return {0.25f, 0.85f, 1.0f};
        case 3:
            return {1.0f, 0.25f, 0.85f};
        case 4:
            return {1.0f, 0.8f, 0.2f};
        default:
            return {1.0f, 1.0f, 1.0f};
    }
}

}  // namespace adapters::gl
