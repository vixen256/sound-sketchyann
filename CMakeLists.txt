cmake_minimum_required(VERSION 3.28)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_SYSTEM_NAME "Windows")
set(CMAKE_SYSTEM_PROCESSOR "x86_64")
set(CMAKE_C_COMPILER "clang")
set(CMAKE_CXX_COMPILER "clang++")

set(CMAKE_C_FLAGS "-ffunction-sections -fdata-sections -flto -pthread -Oz -fuse-ld=lld -masm=intel -D_WIN32_WINNT=_WIN32_WINNT_WIN10 -target x86_64-pc-windows-gnu -g")
set(CMAKE_CXX_FLAGS ${CMAKE_C_FLAGS})
add_link_options(-s -Wl,--gc-sections -Wl,--strip-all -Wl,--as-needed)
add_link_options(-static -static-libgcc -static-libstdc++)

include(FetchContent)

set(BUILD_SHARED_LIBS off)
set(ZYDIS_BUILD_EXAMPLES off)
set(ZYDIS_BUILD_TOOLS off)
set(ZYDIS_BUILD_DOXYGEN off)
set(ZYDIS_BUILD_TESTS off)
set(ZYDIS_MINIMAL_MODE on)
set(ZYDIS_FEATURE_ENCODER off)
set(ZYDIS_FEATURE_FORMATTER off)
set(ZYDIS_FEATURE_AVX512 off)
set(ZYDIS_FEATURE_KNC off)
set(ZYDIS_FEATURE_SEGMENT off)

FetchContent_Declare(
    Zydis
    GIT_REPOSITORY "https://github.com/zyantific/zydis.git"
    GIT_TAG "v4.1.0"
    OVERRIDE_FIND_PACKAGE
)

set(SAFETYHOOK_FETCH_ZYDIS off)
FetchContent_Declare(
    safetyhook
    GIT_REPOSITORY "https://github.com/cursey/safetyhook.git"
    GIT_TAG "origin/main"
)

FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(Zydis safetyhook tomlplusplus)

project(sound LANGUAGES ASM_NASM C CXX)

add_library(sound SHARED
    src/diva.cpp
    src/mod.cpp
)

target_include_directories(sound PUBLIC
    ${CMAKE_SOURCE_DIR}/FFmpeg/
    ${CMAKE_SOURCE_DIR}/FFmpeg/ffmpeg/
)
target_link_libraries(sound PUBLIC
    Zydis
    safetyhook
    tomlplusplus::tomlplusplus
    ${CMAKE_SOURCE_DIR}/FFmpeg/libavcodec/avcodec.lib
    ${CMAKE_SOURCE_DIR}/FFmpeg/libavformat/avformat.lib
    ${CMAKE_SOURCE_DIR}/FFmpeg/libavutil/avutil.lib
    ${CMAKE_SOURCE_DIR}/FFmpeg/libswresample/swresample.lib
    bcrypt
)
target_precompile_headers(sound PUBLIC src/helpers.h)
target_compile_features(sound PUBLIC cxx_std_23)

set_target_properties(
    sound
    PROPERTIES
        PREFIX ""
)

install(
    TARGETS sound
    EXPORT
)
