# `just` lists the recipes; `just render` builds and writes out/render.exr + out/render.png.

build_dir := "build"

# Where the engine's build unpacks Slang and Dawn: one directory outside every build, so all worktrees share a
# single download. NV_TOOLS_DIR overrides it.
tools_dir := env_var_or_default("NV_TOOLS_DIR", env_var_or_default("XDG_CACHE_HOME", env_var("HOME") + "/.cache") + "/nv-tools")

default:
    @just --list

# Check out the engine submodule if it is missing, and use .githooks/ so new worktrees do it themselves.
setup:
    #!/usr/bin/env bash
    # core.hooksPath is shared by every worktree. It is set only while unset, so hooks of your own are never replaced.
    # The clone borrows the main checkout's engine objects when they exist, then copies them (--dissociate) so this
    # worktree does not depend on that checkout. An engine already checked out is left alone, so work in progress
    # there is never reset.
    set -euo pipefail
    hooks="$(git config --get core.hooksPath || true)"
    if [ -z "$hooks" ]; then
        git config core.hooksPath .githooks
    elif [ "$hooks" != ".githooks" ]; then
        echo "heosang: core.hooksPath is '$hooks', not .githooks: new worktrees will not check out the engine themselves" >&2
    fi
    if [ -e engine/.git ]; then exit 0; fi
    main="$(dirname "$(git rev-parse --path-format=absolute --git-common-dir)")"
    if [ -e "$main/engine/.git" ]; then
        reference="$(git -C "$main/engine" rev-parse --absolute-git-dir)"
        git submodule update --init --recursive --reference "$reference" --dissociate
    else
        git submodule update --init --recursive
    fi

# Configure (once) and build the headless renderer.
build: setup
    if [ ! -f {{build_dir}}/CMakeCache.txt ]; then cmake -S . -B {{build_dir}} -G Ninja -DCMAKE_BUILD_TYPE=Release -DNV_TOOLS_DIR="{{tools_dir}}"; fi
    cmake --build {{build_dir}}

# Build, then render the triangle: out/render.exr (linear float) and out/render.png (tone-mapped).
render: build
    ./{{build_dir}}/heosang

# Remove the build and the renders.
clean:
    rm -rf {{build_dir}} out
