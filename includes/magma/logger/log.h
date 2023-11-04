#pragma once

#include <stdint.h>

enum magma_log_levels {
	MAGMA_LOG_NONE,
	MAGMA_INFO,
	MAGMA_DEBUG,
	MAGMA_WARN,
	MAGMA_ERROR,
	MAGMA_FATAL,
	MAGMA_LOG_END
};

void magma_log_set_level(enum magma_log_levels level);
int magma_log(enum magma_log_levels level, const uint32_t line, 
		const char *function, const char *file, const char *fmt, ...);

#define magma_log_info(fmt, ...) magma_log(MAGMA_INFO, __LINE__, __FUNCTION__, __FILE__, fmt, __VA_ARGS__)
#define magma_log_debug(fmt, ...) magma_log(MAGMA_DEBUG, __LINE__, __FUNCTION__, __FILE__, fmt, __VA_ARGS__)
#define magma_log_warn(fmt, ...) magma_log(MAGMA_WARN, __LINE__, __FUNCTION__, __FILE__, fmt, __VA_ARGS__)
#define magma_log_error(fmt, ...) magma_log(MAGMA_ERROR, __LINE__, __FUNCTION__, __FILE__, fmt, __VA_ARGS__)
#define magma_log_fatal(fmt, ...) magma_log(MAGMA_FATAL, __LINE__, __FUNCTION__, __FILE__, fmt, __VA_ARGS__)
