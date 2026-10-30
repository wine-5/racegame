#pragma once

// OpenGL の関数と定数。ブラウザでは WebGL2（OpenGL ES 3.0）、ネイティブでは OpenGL 3.3 core を使う
#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#else
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>
#endif
