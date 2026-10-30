#pragma once

namespace adapters::gl
{

/**
 * @brief 頂点シェーダーとフラグメントシェーダーからプログラムを作る
 * @details ソースの先頭に、ブラウザとネイティブで違う #version 行を付けてからコンパイルする。
 *          失敗したら理由をログに出す。
 * @param vertexSource 頂点シェーダーのソース（#version 行は書かない）
 * @param fragmentSource フラグメントシェーダーのソース（#version 行は書かない）
 * @return プログラムの番号。失敗したら 0
 */
unsigned buildProgram(const char* vertexSource, const char* fragmentSource);

}  // namespace adapters::gl
