#pragma once

// Standalone on purpose: engine/base.h defines `internal` and `global` as macros, which break
// the C++ standard headers image_write.cpp needs. Plain <stdint.h> types it is.

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Writes `width` x `height` RGBA half floats (tightly packed, rows from the top) as they are:
// a linear EXR, the render before tone mapping.
int nv_write_exr(const char* path, const uint16_t* rgba16f, uint32_t width, uint32_t height);

// The same pixels as an 8-bit PNG: exposure 1, the engine's Khronos PBR Neutral tone mapping
// (engine/shaders/upscale.slang), then the sRGB encode the canvas view does on the web.
int nv_write_png_tonemapped(const char* path, const uint16_t* rgba16f, uint32_t width, uint32_t height);

#ifdef __cplusplus
}
#endif
