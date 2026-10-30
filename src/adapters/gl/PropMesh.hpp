#pragma once

#include "adapters/gl/CpuMesh.hpp"
#include "domain/Props.hpp"

namespace adapters::gl
{

// パイロンとドラム缶の見た目。原点は底の中心で、y が上。

/**
 * @brief パイロンのメッシュを作る
 * @details オレンジの円すいに、光を返す白い帯を巻く。
 * @param mesh 作り先
 */
void buildConeMesh(CpuMesh& mesh);

/**
 * @brief ドラム缶のメッシュを作る
 * @details 赤い円柱に、光を返す黄色の帯を巻く。
 * @param mesh 作り先
 */
void buildDrumMesh(CpuMesh& mesh);

/**
 * @brief 小物を描くためのモデル行列を作る
 * @details 弾かれた小物は、底の中心を軸に倒れながら回る。
 * @param prop 小物の状態
 * @return モデル行列
 */
domain::Mat4 propModelMatrix(const domain::Prop& prop);

}  // namespace adapters::gl
