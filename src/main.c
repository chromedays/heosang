// heosang: one still render of a rotating triangle, headless, written as a pair — the linear
// float EXR and the tone-mapped 8-bit PNG of the same pixels (src/image_write.cpp).
//
//   heosang [output-stem] [angle-degrees]
//   heosang out/render 30        (the defaults)

#include "engine/base.h"
#include "engine/gpu.h"
#include "engine/log.h"
#include "engine/window.h"

#include "image_write.h"
#include "nv_version.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// The triangle and its rotation, in one uniform: cos, sin, the aspect correction (height / width,
// so the triangle keeps its shape on a wide frame) and a pad. The vertex colors are HDR on
// purpose (up to 3.0): the EXR holds them as rendered and the PNG's tone mapping rolls them off,
// which is the difference the pair of files exists to show.
internal const char* triangle_wgsl =
    "struct Uniforms { transform: vec4f }\n"
    "@group(0) @binding(0) var<uniform> u: Uniforms;\n"
    "struct Out { @builtin(position) pos: vec4f, @location(0) color: vec3f }\n"
    "@vertex fn vs_main(@builtin(vertex_index) i: u32) -> Out {\n"
    "  var p = array<vec2f, 3>(vec2f(0.0, 0.6), vec2f(-0.6, -0.4), vec2f(0.6, -0.4));\n"
    "  var c = array<vec3f, 3>(vec3f(3.0, 0.35, 0.2), vec3f(0.2, 2.2, 0.5), vec3f(0.25, 0.4, 3.0));\n"
    "  let q = p[i];\n"
    "  let r = vec2f(q.x * u.transform.x - q.y * u.transform.y, q.x * u.transform.y + q.y * u.transform.x);\n"
    "  var out: Out;\n"
    "  out.pos = vec4f(r.x * u.transform.z, r.y, 0.0, 1.0);\n"
    "  out.color = c[i];\n"
    "  return out;\n"
    "}\n"
    "@fragment fn fs_main(in: Out) -> @location(0) vec4f { return vec4f(in.color, 1.0); }\n";

internal f32 half_to_float(u16 h)
{
    u32 sign = (h >> 15) & 1, exp = (h >> 10) & 0x1f, frac = h & 0x3ff;
    u32 bits;
    if (exp == 0) {
        if (!frac)
            bits = sign << 31;
        else {
            int e = -1;
            while (!(frac & 0x400)) {
                frac <<= 1;
                e++;
            }
            frac &= 0x3ff;
            bits = (sign << 31) | ((u32)(127 - 15 - e) << 23) | (frac << 13);
        }
    } else if (exp == 31)
        bits = (sign << 31) | (255u << 23) | (frac << 13);
    else
        bits = (sign << 31) | ((exp - 15 + 127) << 23) | (frac << 13);
    f32 f;
    memcpy(&f, &bits, 4);
    return f;
}

// Makes the folders above `stem` ("out/render" makes "out"), as mkdir -p does for one stem.
internal void make_parent_dirs(const char* stem)
{
    char path[512];
    snprintf(path, sizeof(path), "%s", stem);
    for (char* c = path + 1; *c; ++c) {
        if (*c == '/') {
            *c = 0;
            mkdir(path, 0777);
            *c = '/';
        }
    }
}

int main(int argc, char** argv)
{
    const char* stem = argc > 1 ? argv[1] : "out/render";
    f32 angle_degrees = argc > 2 ? (f32)atof(argv[2]) : 30.0f;
    make_parent_dirs(stem);
    char exr_path[512], png_path[512];
    snprintf(exr_path, sizeof(exr_path), "%s.exr", stem);
    snprintf(png_path, sizeof(png_path), "%s.png", stem);

    NvWindow window = {0};
    nv_window_create(&window, "heosang");
    NvGpu gpu = {0};
    if (!nv_gpu_create(&gpu, &window)) {
        nv_log(NV_LOG_ERROR, "heosang", "no GPU device");
        return 1;
    }
    nv_log(NV_LOG_INFO, "heosang", "render %ux%u, angle %.1f degrees, commit %s (%s)", gpu.width, gpu.height,
           (f64)angle_degrees, NV_GIT_COMMIT, NV_GIT_SUBJECT);

    u32 width = gpu.width, height = gpu.height;
    NvGpuTextureDesc texture_desc = {.label = "render",
                                     .width = width,
                                     .height = height,
                                     .mip_count = 1,
                                     .format = NV_GPU_FORMAT_RGBA16F,
                                     .usage = NV_GPU_TEXTURE_RENDER_TARGET | NV_GPU_TEXTURE_COPY_SRC};
    NvGpuTexture texture = nv_gpu_create_texture(&gpu, &texture_desc);
    NvGpuView view = nv_gpu_create_view(&gpu, texture, NULL);

    f32 radians = angle_degrees * 3.14159265f / 180.0f;
    f32 transform[4] = {cosf(radians), sinf(radians), (f32)height / (f32)width, 0.0f};
    NvGpuBufferDesc uniform_desc = {.label = "triangle uniforms",
                                    .usage = NV_GPU_BUFFER_UNIFORM,
                                    .size = sizeof(transform),
                                    .data = transform};
    NvGpuBuffer uniforms = nv_gpu_create_buffer(&gpu, &uniform_desc);
    NvGpuLayoutEntry layout_entry = {.binding = 0, .stages = NV_GPU_STAGE_VERTEX, .kind = NV_GPU_BINDING_UNIFORM};
    NvGpuLayout layout = nv_gpu_create_layout(&gpu, &layout_entry, 1);
    NvGpuGroupEntry group_entry = {.binding = 0, .buffer = uniforms, .size = sizeof(transform)};
    NvGpuGroup group = nv_gpu_create_group(&gpu, layout, &group_entry, 1);

    NvGpuShaderCode shader = {.wgsl = triangle_wgsl};
    NvGpuRenderPipelineDesc pipeline_desc = {.label = "triangle",
                                             .shader = &shader,
                                             .vertex_entry = "vs_main",
                                             .fragment_entry = "fs_main",
                                             .color_format = NV_GPU_FORMAT_RGBA16F};
    pipeline_desc.layouts[0] = layout;
    pipeline_desc.layout_count = 1;
    NvGpuPipeline pipeline = nv_gpu_create_render_pipeline(&gpu, &pipeline_desc);

    NvGpuFrame frame = nv_gpu_begin_frame(&gpu);
    if (!frame.ok) {
        nv_log(NV_LOG_ERROR, "heosang", "no frame to render into");
        return 1;
    }
    NvGpuRenderPassDesc pass = {.label = "triangle", .color = view, .clear_color = {0.015f, 0.015f, 0.02f, 1.0f}};
    nv_gpu_begin_render_pass(&gpu, &pass);
    nv_gpu_set_pipeline(&gpu, pipeline);
    nv_gpu_set_group(&gpu, 0, group, NULL, 0);
    nv_gpu_draw(&gpu, 3, 1, 0, 0);
    nv_gpu_end_pass(&gpu);
    if (!nv_gpu_copy_texture_for_readback(&gpu, 0, texture))
        nv_log(NV_LOG_WARNING, "heosang", "readback slot busy; the poll below will time out");
    nv_gpu_end_frame(&gpu);

    NvGpuReadback readback = {0};
    for (int tries = 0; tries < 100000 && !readback.ok; ++tries)
        readback = nv_gpu_poll_readback(&gpu, 0);
    if (!readback.ok) {
        nv_log(NV_LOG_ERROR, "heosang", "reading the render back timed out");
        return 1;
    }
    const u16* pixels = readback.bytes;
    if (readback.size != (umm)width * height * 4 * sizeof(u16)) {
        nv_log(NV_LOG_ERROR, "heosang", "readback size %llu, expected %llu", (unsigned long long)readback.size,
               (unsigned long long)((umm)width * height * 4 * sizeof(u16)));
        return 1;
    }
    f32 min_value = 1e30f, max_value = -1e30f;
    for (umm i = 0; i < (umm)width * height * 4; ++i) {
        f32 value = half_to_float(pixels[i]);
        if (value < min_value)
            min_value = value;
        if (value > max_value)
            max_value = value;
    }
    nv_log(NV_LOG_INFO, "heosang", "readback: linear range %.4f .. %.4f", (f64)min_value, (f64)max_value);

    b32 ok = nv_write_exr(exr_path, pixels, width, height) && nv_write_png_tonemapped(png_path, pixels, width, height);
    if (!ok)
        return 1;
    nv_log(NV_LOG_INFO, "heosang", "wrote %s and %s", exr_path, png_path);
    return 0;
}
