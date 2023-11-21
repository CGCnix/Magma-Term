#include <magma/logger/log.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>

static enum magma_log_levels log_level;

static const char *magma_log_level_str(enum magma_log_levels level) {
	switch (level) {
		case MAGMA_MEMINFO:
			return "MEMINFO";
		case MAGMA_INFO:
			return "INFO";
		case MAGMA_WARN:
			return "WARN";
		case MAGMA_DEBUG:
			return "DEBUG";
		case MAGMA_ERROR:
			return "ERROR";
		case MAGMA_FATAL:
			return "FATAL";
		default:
			return "Unknown";
	}
}

static const char *magma_log_level_color(enum magma_log_levels level) {
	switch (level) {
		case MAGMA_MEMINFO:
			return "\x1b[1;35m";
		case MAGMA_INFO:
			return "\x1b[1;36m";
		case MAGMA_WARN:
			return "\x1b[1;33m";
		case MAGMA_DEBUG:
			return "\x1b[1;32m";
		case MAGMA_ERROR:
			return "\x1b[1;31m";
		case MAGMA_FATAL:
			return "\x1b[1;41m";
		default:
			return "\x1b[1;34m";
	}
}

void magma_log_set_level(enum magma_log_levels level) {
	log_level = level;
}

int magma_log(enum magma_log_levels level, const uint32_t line, 
		const char *file, const char *fmt, ...) {
	int len;
	va_list args;

	if(level < log_level) return 0;

	printf("%s%s\x1b[0m \x1b[1;35m%s(%d):\x1b[0m \x1b[32m\x1b[0m ", magma_log_level_color(level), magma_log_level_str(level), file, line);

	va_start(args, fmt);
	len = vprintf(fmt, args);
	va_end(args);

	return len;
}

int magma_log_printf(enum magma_log_levels level, const char *fmt, ...) {
	int len;
	va_list args;

	if(level < log_level) return 0;

	va_start(args, fmt);
	len = vprintf(fmt, args);
	va_end(args);
	
	return len;
}
