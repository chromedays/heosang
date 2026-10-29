// The render written two ways: the linear float EXR (OpenEXR, the pixels as rendered) and the
// 8-bit PNG (libpng, after tone mapping and the sRGB encode). Links system libraries on purpose:
// image file formats are not this project's point, as the engine says of its own choices.

#include "image_write.h"

#include <Imath/half.h>
#include <OpenEXR/ImfChannelList.h>
#include <OpenEXR/ImfFrameBuffer.h>
#include <OpenEXR/ImfHeader.h>
#include <OpenEXR/ImfOutputFile.h>
#include <png.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <exception>

namespace {

float to_float(uint16_t half_bits)
{
    half value;
    value.setBits(half_bits);
    return value;
}

// Khronos PBR Neutral, ported line for line from engine/shaders/upscale.slang (Apache-2.0,
// github.com/KhronosGroup/ToneMapping), which is where the engine itself tone-maps for display.
void tone_pbr_neutral(float rgb[3])
{
    const float start_compression = 0.8f - 0.04f;
    const float desaturation = 0.15f;
    float x = std::fmin(rgb[0], std::fmin(rgb[1], rgb[2]));
    float offset = x < 0.08f ? x - 6.25f * x * x : 0.04f;
    for (int c = 0; c < 3; ++c)
        rgb[c] -= offset;
    float peak = std::fmax(rgb[0], std::fmax(rgb[1], rgb[2]));
    if (peak < start_compression)
        return;
    float d = 1.0f - start_compression;
    float new_peak = 1.0f - d * d / (peak + d - start_compression);
    for (int c = 0; c < 3; ++c)
        rgb[c] *= new_peak / peak;
    float g = 1.0f - 1.0f / (desaturation * (peak - new_peak) + 1.0f);
    for (int c = 0; c < 3; ++c)
        rgb[c] += (new_peak - rgb[c]) * g;
}

float srgb_encode(float linear)
{
    if (linear <= 0.0f)
        return 0.0f;
    return linear <= 0.0031308f ? 12.92f * linear : 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f;
}

} // namespace

extern "C" int nv_write_exr(const char* path, const uint16_t* rgba16f, uint32_t width, uint32_t height)
{
    try {
        Imf::Header header((int)width, (int)height, 1.0f, Imath::V2f(0, 0), 1.0f,
                           Imf::INCREASING_Y, Imf::ZIP_COMPRESSION);
        header.channels().insert("R", Imf::Channel(Imf::HALF, 1, 1));
        header.channels().insert("G", Imf::Channel(Imf::HALF, 1, 1));
        header.channels().insert("B", Imf::Channel(Imf::HALF, 1, 1));
        header.channels().insert("A", Imf::Channel(Imf::HALF, 1, 1));
        char* base = const_cast<char*>((const char*)rgba16f);
        const size_t pixel_bytes = 4 * sizeof(uint16_t);
        Imf::FrameBuffer frame;
        frame.insert("R", Imf::Slice(Imf::HALF, base + 0 * sizeof(uint16_t), pixel_bytes, pixel_bytes * width));
        frame.insert("G", Imf::Slice(Imf::HALF, base + 1 * sizeof(uint16_t), pixel_bytes, pixel_bytes * width));
        frame.insert("B", Imf::Slice(Imf::HALF, base + 2 * sizeof(uint16_t), pixel_bytes, pixel_bytes * width));
        frame.insert("A", Imf::Slice(Imf::HALF, base + 3 * sizeof(uint16_t), pixel_bytes, pixel_bytes * width));
        Imf::OutputFile file(path, header);
        file.setFrameBuffer(frame);
        file.writePixels((int)height);
        return 1;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "writing %s failed: %s\n", path, error.what());
        return 0;
    }
}

extern "C" int nv_write_png_tonemapped(const char* path, const uint16_t* rgba16f, uint32_t width, uint32_t height)
{
    FILE* file = std::fopen(path, "wb");
    if (!file) {
        std::fprintf(stderr, "writing %s failed: cannot open\n", path);
        return 0;
    }
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    png_infop info = png ? png_create_info_struct(png) : nullptr;
    if (!png || !info || setjmp(png_jmpbuf(png))) {
        std::fprintf(stderr, "writing %s failed in libpng\n", path);
        if (png)
            png_destroy_write_struct(&png, &info);
        std::fclose(file);
        return 0;
    }
    png_init_io(png, file);
    png_set_IHDR(png, info, width, height, 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);
    uint8_t* row = new uint8_t[(size_t)width * 4];
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const uint16_t* pixel = rgba16f + ((size_t)y * width + x) * 4;
            float rgb[3] = {to_float(pixel[0]), to_float(pixel[1]), to_float(pixel[2])};
            for (int c = 0; c < 3; ++c)
                rgb[c] = std::fmax(rgb[c] * 1.0f, 0.0f); // the exposure multiply, 1.0
            tone_pbr_neutral(rgb);
            for (int c = 0; c < 3; ++c) {
                float clamped = std::fmin(std::fmax(rgb[c], 0.0f), 1.0f);
                row[x * 4 + c] = (uint8_t)(srgb_encode(clamped) * 255.0f + 0.5f);
            }
            float alpha = std::fmin(std::fmax(to_float(pixel[3]), 0.0f), 1.0f);
            row[x * 4 + 3] = (uint8_t)(alpha * 255.0f + 0.5f);
        }
        png_write_row(png, row);
    }
    delete[] row;
    png_write_end(png, info);
    png_destroy_write_struct(&png, &info);
    std::fclose(file);
    return 1;
}
