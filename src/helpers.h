#include <bits/stdc++.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <safetyhook.hpp>
#include <shellapi.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#define TOML_EXCEPTIONS                 0
#define TOML_ENABLE_UNRELEASED_FEATURES 1
#include <toml++/toml.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef float f32;
typedef double f64;

#define INSERT_PADDING(length) u8 pad##__LINE__[length]

#define FUNCTION_PTR(returnType, function, location, ...) returnType (*function) (__VA_ARGS__) = (returnType (*) (__VA_ARGS__)) (location)
#define FUNCTION_PTR_H(returnType, function, ...)         extern returnType (*function) (__VA_ARGS__)

#define PROC_ADDRESS(libraryName, procName) GetProcAddress (LoadLibrary (TEXT (libraryName)), procName)

#define READ_MEMORY(location, type) *(type *)location

#define WRITE_MEMORY(location, type, ...)                                                        \
	{                                                                                            \
		const type data[] = {__VA_ARGS__};                                                       \
		DWORD oldProtect;                                                                        \
		VirtualProtect ((void *)(location), sizeof (data), PAGE_EXECUTE_READWRITE, &oldProtect); \
		memcpy ((void *)(location), data, sizeof (data));                                        \
		VirtualProtect ((void *)(location), sizeof (data), oldProtect, &oldProtect);             \
	}

#define WRITE_NOP(location, count)                                                                 \
	{                                                                                              \
		DWORD oldProtect;                                                                          \
		VirtualProtect ((void *)(location), (size_t)(count), PAGE_EXECUTE_READWRITE, &oldProtect); \
		for (size_t i = 0; i < (size_t)(count); i++)                                               \
			*((uint8_t *)(location) + i) = 0x90;                                                   \
		VirtualProtect ((void *)(location), (size_t)(count), oldProtect, &oldProtect);             \
	}

#define WRITE_NULL(location, count)                                                                \
	{                                                                                              \
		DWORD oldProtect;                                                                          \
		VirtualProtect ((void *)(location), (size_t)(count), PAGE_EXECUTE_READWRITE, &oldProtect); \
		for (size_t i = 0; i < (size_t)(count); i++)                                               \
			*((uint8_t *)(location) + i) = 0x00;                                                   \
		VirtualProtect ((void *)(location), (size_t)(count), oldProtect, &oldProtect);             \
	}

#define COUNTOFARR(arr) sizeof (arr) / sizeof (arr[0])
