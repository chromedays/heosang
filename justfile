# `just` lists the recipes; `just render` builds and writes out/render.exr + out/render.png.

build_dir := "build"

default:
    @just --list

# Configure (once) and build the headless renderer.
build:
    if [ ! -f {{build_dir}}/CMakeCache.txt ]; then cmake -S . -B {{build_dir}} -G Ninja -DCMAKE_BUILD_TYPE=Release; fi
    cmake --build {{build_dir}}

# Build, then render the triangle: out/render.exr (linear float) and out/render.png (tone-mapped).
render: build
    ./{{build_dir}}/heosang

# Remove the build and the renders.
clean:
    rm -rf {{build_dir}} out
