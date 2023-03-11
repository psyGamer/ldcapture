#pragma once

#include "base.h"

#define LOG_WARN_ENABLED 1
#define LOG_INFO_ENABLED 1
#define LOG_DEBUG_ENABLED 1
#define LOG_TRACE_ENABLED 1

// Disable debug and trace logging for release builds.
#if RELEASE == 1
	#define LOG_DEBUG_ENABLED 0
	#define LOG_TRACE_ENABLED 0
#endif

typedef enum log_level_t
{
    LOG_LEVEL_FATAL = 0,
    LOG_LEVEL_ERROR = 1,
    LOG_LEVEL_WARN = 2,
    LOG_LEVEL_INFO = 3,
    LOG_LEVEL_DEBUG = 4,
    LOG_LEVEL_TRACE = 5
} log_level_t;

void log_output(log_level_t level, const char* message, ...);

// Logs a fatal-level message.
#define FATAL(message, ...) log_output(LOG_LEVEL_FATAL, message, ##__VA_ARGS__)

#ifndef ERROR
	// Logs an error-level message.
	#define ERROR(message, ...) log_output(LOG_LEVEL_ERROR, message, ##__VA_ARGS__)
#endif

#if LOG_WARN_ENABLED == 1
	// Logs a warning-level message.
	#define WARN(message, ...) log_output(LOG_LEVEL_WARN, message, ##__VA_ARGS__)
#else
	// Does nothing when LOG_WARN_ENABLED != 1
	#define WARN(message, ...)
#endif

#if LOG_INFO_ENABLED == 1
	// Logs a info-level message.
	#define INFO(message, ...) log_output(LOG_LEVEL_INFO, message, ##__VA_ARGS__)
#else
	// Does nothing when LOG_INFO_ENABLED != 1
	#define INFO(message, ...)
#endif

#if LOG_DEBUG_ENABLED == 1
	// Logs a debug-level message.
	#define DEBUG(message, ...) log_output(LOG_LEVEL_DEBUG, message, ##__VA_ARGS__)
#else
	// Does nothing when LOG_DEBUG_ENABLED != 1
	#define DEBUG(message, ...)
#endif

#if LOG_TRACE_ENABLED == 1
	// Logs a trace-level message.
	#define TRACE(message, ...) log_output(LOG_LEVEL_TRACE, message, ##__VA_ARGS__)
#else
	// Does nothing when LOG_TRACE_ENABLED != 1
	#define TRACE(message, ...)
#endif