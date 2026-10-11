#include <Zydis/Zydis.h>
#include <bits/stdc++.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <psapi.h>
#include <qis/signature.hpp>
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

void *sig_scan (const char *signature, std::initializer_list<u64> hints);
void *sig_scan_xref (const char *signature, std::initializer_list<u64> hints);

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

#define HOOK(returnType, name, location, ...)         \
	typedef returnType (*typeof##name) (__VA_ARGS__); \
	SafetyHookInline hook##name{};                    \
	typeof##name original##name = nullptr;            \
	void *where##name           = (void *)location;   \
	returnType impl##name (__VA_ARGS__)

#define INSTALL_HOOK(name)                                                \
	hook##name     = safetyhook::create_inline (where##name, impl##name); \
	original##name = hook##name.original<typeof##name> ();

#define MIDHOOK(name, location)           \
	SafetyHookMid hook##name;             \
	void *where##name = (void *)location; \
	void impl##name (SafetyHookContext &ctx)

#define INSTALL_MIDHOOK(name) hook##name = safetyhook::create_mid (where##name, impl##name);

#define SIG_SCAN(name, pattern, ...)                                                  \
	void *name ();                                                                    \
	void *name##Addr = name ();                                                       \
	void *name () {                                                                   \
		if (!name##Addr) {                                                            \
			std::initializer_list<u64> name##Hints = {__VA_ARGS__};                   \
			name##Addr                             = sig_scan (pattern, name##Hints); \
		}                                                                             \
		return name##Addr;                                                            \
	}

#define SIG_SCAN_XREF(name, pattern, ...)                                                  \
	void *name ();                                                                         \
	void *name##Addr = name ();                                                            \
	void *name () {                                                                        \
		if (!name##Addr) {                                                                 \
			std::initializer_list<u64> name##Hints = {__VA_ARGS__};                        \
			name##Addr                             = sig_scan_xref (pattern, name##Hints); \
		}                                                                                  \
		return name##Addr;                                                                 \
	}
