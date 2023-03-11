#pragma once

#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "logger.h"

// Rust-like primitive type names
typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float  f32;
typedef double f64;

// Platform detection
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
	// Windows 64-bit
	#define PLATFORM_WINDOWS 1
	#ifndef _WIN64
		#error "64-bit is required on Windows!"
	#endif
#elif defined(__linux__) || defined(__gnu_linux__)
	// Linux
	#define PLATFORM_LINUX 1
	#if defined(__ANDROID__)
		#define PLATFORM_ANDROID 1
		#error "Android is not supported!"
	#endif
#elif defined(__unix__)
	// Catch anything not caught by the above.
	#define PLATFORM_UNIX 1
#elif defined(_POSIX_VERSION)
	// Posix
	#define PLATFORM_POSIX 1
#elif __APPLE__
	// Apple platforms
	#define PLATFORM_APPLE 1
	#include <TargetConditionals.h>
	#if TARGET_IPHONE_SIMULATOR
		// iOS Simulator
		#define PLATFORM_IOS 1
		#define PLATFORM_IOS_SIMULATOR 1
        #error "iOS Simulator is not supported!"
	#elif TARGET_OS_IPHONE
		// iOS device
		#define PLATFORM_IOS 1
        #error "iOS is not supported!"
	#elif TARGET_OS_MAC
		// Other kinds of Mac OS
        #error "MacOS Simulator is not supported!"
	#else
		#error "Unknown Apple platform"
	#endif
#else
	#error "Unknown platform!"
#endif