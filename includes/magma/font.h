#pragma once

#include <ft2build.h>
#include <stdint.h>
#include FT_FREETYPE_H

#include <fontconfig/fontconfig.h>


typedef struct magma_font {
	uint32_t height;
	
	uint32_t ascent, descent;

	struct advance {
		uint32_t x;
		uint32_t y;
	} advance;

	FcConfig *font_config;
	FT_Library ft_lib;
	FT_Face face;
} magma_font_t;

magma_font_t *magma_font_init(const char *fconfig_str);
void magma_font_deinit(magma_font_t *font);
