# heosang

A headless still renderer built on the [nv engine](https://github.com/chromedays/engine)
(`chromedays/engine`, added as the `engine/` Git submodule). It renders one triangle on
Linux through Dawn's native WebGPU — no browser, no Emscripten, no window — and writes
the render as a pair:

- `out/render.exr` — the linear float render (half-float RGBA, as rendered)
- `out/render.png` — the same pixels as an 8-bit PNG, tone-mapped (Khronos PBR Neutral,
  the engine's own operator) and sRGB-encoded

## Build and run

Needs CMake 3.30+, Ninja, and the OpenEXR and libpng dev packages
(`pkg-config` finds them). The engine's build downloads its pinned tools
(Slang, Dawn) on first configure.

```sh
git clone --recurse-submodules <this repo>
cd heosang
just render        # or: cmake -S . -B build -G Ninja && cmake --build build && ./build/heosang
```

`./build/heosang [output-stem] [angle-degrees]` renders at another angle or to another
stem (default `out/render`, 30 degrees). The frame is 1920x1080 unless
`NV_HEADLESS_WIDTH` / `NV_HEADLESS_HEIGHT` say otherwise. A Vulkan-capable GPU is used
through Dawn; logs on stderr name the adapter that rendered.

## Layout

- `engine/` — the nv engine submodule (branch `linux-headless` carries the Linux port)
- `src/main.c` — the triangle, one frame, readback
- `src/image_write.cpp` — the EXR and PNG writers (system OpenEXR and libpng)
