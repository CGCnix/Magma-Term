#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

#include <poll.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_BITMAP_H
#include <fontconfig/fontconfig.h>

#include <magma/logger/log.h>
#include <magma/backend/backend.h>

#include <magma/vt.h>
/* TODO: This entire file needs a 
 * redo and the structures need 
 * complete overall to be honest
 */

typedef struct magma_ctx {
	magma_vt_t *vt;

	magma_backend_t *backend;

	FcConfig *fcconfig;

	FT_Library library;
	FT_Face face;

	uint32_t maxascent, maxdescent;

	uint32_t width, height, x, y;

	bool is_running;
} magma_ctx_t;

int glyph_check_bit(const FT_GlyphSlot glyph, const int x, const int y)
{
    int pitch = glyph->bitmap.pitch;
    unsigned char *row = &glyph->bitmap.buffer[pitch * y];
    char cValue = row[x >> 3];

    return (cValue & (128 >> (x & 7))) != 0;
}

void echo_char(magma_ctx_t *ctx, int ch, int x, int y, uint32_t width, unsigned int maxAscent, uint32_t *data2) {
	FT_Bitmap *bitmap;

	if(ch == '\r') {
		return;
	}
	FT_UInt glyphindex = FT_Get_Char_Index(ctx->face, ch);


	FT_Load_Glyph(ctx->face, glyphindex, FT_LOAD_DEFAULT);
	FT_Render_Glyph(ctx->face->glyph, FT_RENDER_MODE_MONO);
	
	bitmap = &ctx->face->glyph->bitmap;

	if(ctx->vt->lines[y][x].attributes == 1) {
		FT_Bitmap_Embolden(ctx->library, bitmap, 1 << 6, 1 << 6);
	}

	for(int yp = 0; yp < bitmap->rows; yp++) {
		for(int xp = 0; xp < bitmap->width; xp++) {
			if(glyph_check_bit(ctx->face->glyph, xp, yp)) {
				
				data2[(20 + (yp + y * 14) + (ctx->maxascent - ctx->face->glyph->bitmap_top)) * width + xp + (x * 8) + ctx->face->glyph->bitmap_left] = ctx->vt->lines[y][x].fg;
			}
		}
	}
}

void draw_cb(magma_backend_t *backend, uint32_t height, uint32_t width, void *data) {
	if(width == 0) return;
	magma_ctx_t *ctx = data;
	uint32_t *data2;

	width = ctx->width;
	height = ctx->height;

	magma_buf_t buf = {
		.width = width,
		.height = height,
		.bpp = 32,
		.depth = 24,
		.buffer = NULL,
	};


	buf.buffer = calloc(4, width * height);
	data2 = buf.buffer;
	for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
			data2[y * width + x] = 0xa02a3628;
        }
    }
	
	for(int y = 0; y < ctx->vt->buf_y + 1; y++) {
		for(int x = 0; x < ctx->vt->cols; x++) {
			if(ctx->vt->lines[y][x].unicode == '\n' || (y == ctx->vt->buf_y && x == ctx->vt->buf_x)) {
				break;
			}
			echo_char(ctx, ctx->vt->lines[y][x].unicode, x, y, width, ctx->maxascent, data2);
		}
	}
	magma_backend_put_buffer(backend, &buf);
}

void key_cb(magma_backend_t *backend, char *utf8, int length, void *data){
	magma_ctx_t *ctx = data;
	magma_log_info("Got to Key callback %d\n", length);	
	write(ctx->vt->master, utf8, length);

}

void fc_get_ascent_and_descent(magma_ctx_t *ctx) {
	FT_GlyphSlot glyph;
	for(int i = 32; i < UINT16_MAX; i++) {
		FT_UInt glyphindex = FT_Get_Char_Index(ctx->face, i);
		FT_Load_Glyph(ctx->face, glyphindex, FT_LOAD_DEFAULT);
		FT_Render_Glyph(ctx->face->glyph, FT_RENDER_MODE_NORMAL);
		glyph = ctx->face->glyph;

		if(ctx->maxascent < ctx->face->glyph->bitmap_top) {
			ctx->maxascent = ctx->face->glyph->bitmap_top;
		}
		if ((glyph->metrics.height >> 6) - glyph->bitmap_top > (int)ctx->maxdescent) {
    		ctx->maxdescent = (glyph->metrics.height >> 6) - glyph->bitmap_top;
		}
	}

}


void resize_cb(magma_backend_t *backend, uint32_t height, uint32_t width, void *data) {
	magma_ctx_t *ctx = data;
	struct winsize ws;
	ws.ws_xpixel = width;
	ws.ws_ypixel = height;
	ws.ws_col = width / 10;
	ws.ws_row = height / 10;

	ctx->vt->lines = realloc(ctx->vt->lines, sizeof(void*) * ws.ws_row);


	for(int i = 0; i < ctx->vt->rows; i++) {
		ctx->vt->lines[i] = realloc(ctx->vt->lines[i], ws.ws_col * sizeof(glyph_t));
	}

	for(int i = ctx->vt->rows; i < ws.ws_row; i++) {
		ctx->vt->lines[i] = calloc(sizeof(glyph_t), ws.ws_col);
	}

	ctx->vt->rows = ws.ws_row;
	ctx->vt->cols = ws.ws_col;
	ctx->width = width;
	ctx->height = height;

	if(ioctl(ctx->vt->master, TIOCSWINSZ, &ws)) {
		printf("Failed to update term size\n");
	}
}

void font_init(magma_ctx_t *ctx, char *font) {
	char *font_file;
	FcPattern *pattern, *font_pattern;
	FcResult result;
	FcChar8 *fc_file;

	ctx->fcconfig = FcInitLoadConfigAndFonts();
	pattern = FcNameParse((const FcChar8*)font);
	
	FcConfigSubstitute(ctx->fcconfig, pattern, FcMatchPattern);
	FcDefaultSubstitute(pattern);
	
	font_pattern = FcFontMatch(ctx->fcconfig, pattern, &result);
	
	if(FcPatternGetString(font_pattern, FC_FILE, 0, &fc_file) == FcResultMatch) {
		
		font_file = (char *)fc_file;
	} else {
		return;
	}
	
	FT_Init_FreeType(&ctx->library);
	FT_New_Face(ctx->library, font_file, 0, &ctx->face);
	

	FcPatternDestroy(font_pattern);
	FcPatternDestroy(pattern);
}

void on_close(magma_backend_t *backend, void *data) {
	magma_ctx_t *ctx = data;


	ctx->is_running = 0;
}

int main(int argc, char **argv) {
	int slave;
	magma_ctx_t ctx = { 0 };
	struct pollfd pfd;

	ctx.vt = calloc(1, sizeof(*ctx.vt));
	ctx.vt->lines = calloc(sizeof(void *), 25);
	for(int i = 0; i < 25; i++) {
		ctx.vt->lines[i] = calloc(sizeof(glyph_t), 80);
	}
	ctx.vt->cols = 80;
	ctx.vt->rows = 25;
	ctx.vt->fg = 0xf8f8f2;	
	__builtin_dump_struct(ctx.vt, &printf);

	if(magma_get_pty(&ctx.vt->master, &slave) < 0) {
		return -1;
	}

	ctx.width = 0;
	ctx.height = 0;
	ctx.is_running = 1;	
	font_init(&ctx, "NotoMono Nerd Font-13px");

	fc_get_ascent_and_descent(&ctx);

	FT_Set_Pixel_Sizes(ctx.face, 14, 14);
	if(magma_fork_pty(ctx.vt->master, &slave) < 0) {
		magma_log_info("Failed to fork\n");
		return 1;
	}

	ctx.backend = magma_backend_init_auto();

	magma_backend_set_on_resize(ctx.backend, resize_cb, &ctx);
	magma_backend_set_on_draw(ctx.backend, draw_cb, &ctx);
	magma_backend_set_on_key(ctx.backend, key_cb, &ctx);
	magma_backend_set_on_close(ctx.backend, on_close, &ctx);

	magma_backend_start(ctx.backend);

	pfd.fd = ctx.vt->master;
	pfd.events = POLLIN;
	while(ctx.is_running) {
		magma_backend_dispatch_events(ctx.backend);
	
		if(poll(&pfd, 1, 10)) {
			if(pfd.revents & POLLERR || pfd.revents & POLLHUP) {
				printf("Child is process has closed\n");
				ctx.is_running = 0;
			}

			vt_read_input(ctx.vt);
		}
		draw_cb(ctx.backend, ctx.height, ctx.width, &ctx);

	}

	magma_backend_dispatch_events(ctx.backend);
	magma_backend_deinit(ctx.backend);
	
	FT_Done_Face(ctx.face);
	FT_Done_FreeType(ctx.library);
	
	FcConfigDestroy(ctx.fcconfig);
	FcFini();
	return 0;
}
