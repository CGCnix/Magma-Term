#pragma once

#include <stdint.h>

enum magma_log_levels {
	MAGMA_LOG_NONE,
	MAGMA_MEMINFO, /*print memory Alloc, free, and realloc from vulkan*/
	MAGMA_INFO,
	MAGMA_DEBUG,
	MAGMA_WARN,
	MAGMA_ERROR,
	MAGMA_FATAL,
	MAGMA_LOG_END
};

void magma_log_set_level(enum magma_log_levels level);
int magma_log(enum magma_log_levels level, const uint32_t line, 
			  const char *file, const char *fmt, ...);
int magma_log_printf(enum magma_log_levels level, const char *fmt, ...);
#define magma_log_meminfo(...) magma_log(MAGMA_MEMINFO, __LINE__, __FILE__, __VA_ARGS__)
#define magma_log_info(...) magma_log(MAGMA_INFO, __LINE__, __FILE__, __VA_ARGS__)
#define magma_log_debug(...) magma_log(MAGMA_DEBUG, __LINE__, __FILE__, __VA_ARGS__)
#define magma_log_warn(...) magma_log(MAGMA_WARN, __LINE__, __FILE__, __VA_ARGS__)
#define magma_log_error(...) magma_log(MAGMA_ERROR, __LINE__, __FILE__, __VA_ARGS__)
#define magma_log_fatal(...) magma_log(MAGMA_FATAL, __LINE__, __FILE__, __VA_ARGS__)
