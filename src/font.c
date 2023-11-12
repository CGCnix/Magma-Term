/*Fontconfig/Freetype*/
#include <freetype2/ft2build.h>
#include <string.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_BITMAP_H

#include <fontconfig/fontconfig.h>

#include <magma/font.h>
#include <magma/logger/log.h>

#include <stdlib.h>

/*Find font or subsitute from fconfig_str*/
static char *magma_fconfig_find_sub(FcConfig *config, const char *fconfig_str) {
	FcPattern *pattern, *font;
	FcChar8 *fc_file;
	FcResult result;
	char *font_file;

	font_file = NULL;

	pattern = FcNameParse((const FcChar8*)fconfig_str);
	FcConfigSubstitute(config, pattern, FcMatchPattern);
	FcDefaultSubstitute(pattern);

	font = FcFontMatch(config, pattern, &result);
	if(font) {	
		if(FcPatternGetString(font, FC_FILE, 0, &fc_file) == FcResultMatch) {
			font_file = strdup((char*)fc_file);
			magma_log_debug("Font file used: %s\n", font_file);
		}
	}
	FcPatternDestroy(font);
	FcPatternDestroy(pattern);
	return font_file;
}

magma_font_t *magma_font_init(const char *fconfig_str) {
	FT_Error ft_error;
	const char *ft_error_str;
	char *font_file;
	magma_font_t *font;
	

	font = calloc(1, sizeof(magma_font_t));
	if(!font) {
		magma_log_error("Failed to allocate font structure\n");
		goto err_font_alloc;
	}

	ft_error = FT_Init_FreeType(&font->ft_lib);
	if(ft_error) {
		ft_error_str = FT_Error_String(ft_error);
		magma_log_error("Failed to init freetype library: %s\n", ft_error_str);
		goto err_ft_init;
	}

	font->font_config = FcInitLoadConfigAndFonts();
	if(!font->font_config) {
		magma_log_error("Failed to load FC config and fonts\n");
		goto err_fc_config_and_fonts;
	}

	font_file = magma_fconfig_find_sub(font->font_config, fconfig_str);
	if(!font_file) {
		magma_log_error("Failed to find fallback font for %s\n", fconfig_str);
		goto err_font_file;
	}
	
	magma_log_debug("%s\n", font_file);
	ft_error = FT_New_Face(font->ft_lib, font_file, 0, &font->face);
	if(ft_error) {
		magma_log_error("Failed to create FT Face: %d\n", FT_Error_String(ft_error));
	}

	/*2 issues here.
	 * if the user calls FT_Set_Pixel_Size these values will be wrong
	 * Or if the users a vertical font but we don't support that anyway
	 * so for now thats fine 
	 */
	font->advance.x = font->face->max_advance_width >> 6;
	font->height = font->face->max_advance_height >> 6;
	return font;

err_font_file:
err_fc_config_and_fonts:
	FT_Done_FreeType(font->ft_lib);
err_ft_init:
	free(font);
err_font_alloc:
	return NULL;
}

void magma_font_deinit(magma_font_t *font) {

	FT_Done_Face(font->face);

	FcConfigDestroy(font->font_config);

	FT_Done_FreeType(font->ft_lib);
}
