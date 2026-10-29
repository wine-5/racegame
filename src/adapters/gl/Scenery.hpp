#pragma once

namespace adapters::gl
{

/**
 * @brief ステージの見た目の種類
 * @details 同じコースの形でも、景色・空・光の当たり方はこれで決める。
 */
enum class Scenery
{
    Circuit,  // 昼のサーキット（芝・縁石・コンクリートの壁）
    Port,     // 夜の港（コンテナ・照明・海）
    Skidpad   // 夜の港の中の練習場
};

}  // namespace adapters::gl
