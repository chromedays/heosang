cmake_minimum_required(VERSION 3.30)
project(heosang LANGUAGES C CXX)

# The engine as a Git submodule (engine/, the linux-headless branch): only its `nv` library is
# built, and it hands this project the platform and compile options below.
add_subdirectory(engine)
add_compile_options(${NV_COMPILE_OPTIONS})
add_compile_definitions(${NV_COMPILE_DEFINITIONS})

# The image writers: OpenEXR for the linear float render, libpng for the tone-mapped PNG.
find_package(PkgConfig REQUIRED)
pkg_check_modules(OPENEXR REQUIRED IMPORTED_TARGET OpenEXR)
pkg_check_modules(LIBPNG REQUIRED IMPORTED_TARGET libpng)

add_executable(heosang src/main.c src/image_write.cpp)
target_link_libraries(heosang PRIVATE nv PkgConfig::OPENEXR PkgConfig::LIBPNG)
nv_setup_executable(heosang ROOT)
nv_add_version(heosang)
