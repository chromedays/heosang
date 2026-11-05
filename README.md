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
(Slang, Dawn) on first configure; `just build` keeps them in `~/.cache/nv-tools`
(`NV_TOOLS_DIR` overrides it), so every checkout and worktree shares one download.

```sh
git clone --recurse-submodules <this repo>
cd heosang
just render        # or: cmake -S . -B build -G Ninja && cmake --build build && ./build/heosang
```

`./build/heosang [output-stem] [angle-degrees]` renders at another angle or to another
stem (default `out/render`, 30 degrees). The frame is 1920x1080 unless
`NV_HEADLESS_WIDTH` / `NV_HEADLESS_HEIGHT` say otherwise. A Vulkan-capable GPU is used
through Dawn; logs on stderr name the adapter that rendered.

### Worktrees

`git worktree add` does not check out submodules. `just setup` does it, and `just build`
runs it first: it clones `engine/` borrowing the main checkout's objects, then copies
them so the worktree stands alone, and leaves an engine that is already checked out
untouched. It also points `core.hooksPath` at `.githooks/` when nothing else is set
there (it only warns when something is), so after the first build in any checkout, the
`post-checkout` hook runs `just setup` in every new worktree. A new worktree still
builds from scratch, but downloads only the small FetchContent sources and the engine's
LFS assets.

## Layout

- `engine/` — the nv engine submodule (branch `linux-headless` carries the Linux port)
- `src/main.c` — the triangle, one frame, readback
- `src/image_write.cpp` — the EXR and PNG writers (system OpenEXR and libpng)
- `.githooks/post-checkout` — checks out the engine in a new worktree (`just setup` enables it)
