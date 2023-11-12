#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <time.h>
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
#include <magma/font.h>
/* TODO: This entire file needs a 
 * redo and the structures need 
 * complete overall to be honest
 */

#define UNUSED(x) ((void)x)

typedef struct magma_ctx {
	magma_vt_t *vt;

	magma_backend_t *backend;


	magma_font_t *font;
	
	int32_t maxascent, maxdescent;

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

void echo_char(magma_ctx_t *ctx, int ch, int x, int y, uint32_t *data2) {
	FT_Bitmap *bitmap;
	FT_GlyphSlot glyph;
	FT_UInt glyphindex;
	uint32_t yp, xp, xoff, ypos, xpos, yoff;

	if(ch == '\r') {
		return;
	}

	xoff = x * ctx->font->advance.x;
	yoff = (y * ctx->font->height) + ctx->font->ascent;

	glyphindex = FT_Get_Char_Index(ctx->font->face, ch);

	FT_Load_Glyph(ctx->font->face, glyphindex, FT_LOAD_DEFAULT);
	FT_Render_Glyph(ctx->font->face->glyph, FT_RENDER_MODE_MONO);
	
	bitmap = &ctx->font->face->glyph->bitmap;
	glyph = ctx->font->face->glyph;
	if(ctx->vt->lines[y][x].attributes == 1) {
		FT_Bitmap_Embolden(ctx->font->ft_lib, bitmap, 1 << 6, 1 << 6);
	}

	for(yp = 0; yp < bitmap->rows; yp++) {
		for(xp = 0; xp < bitmap->width; xp++) {
			if(glyph_check_bit(glyph, xp, yp)) {
				ypos = yp + yoff - glyph->bitmap_top; 
				xpos = xp + (xoff + glyph->bitmap_left);
				data2[ypos * ctx->width + xpos] = ctx->vt->lines[y][x].fg;
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
	for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
			data2[y * width + x] = 0xa02a3628;
        }
    }
	
	for(int y = 0; y <= ctx->vt->buf_y; y++) {
		for(int x = 0; x < ctx->vt->cols; x++) {
			if(ctx->vt->lines[y][x].unicode == '\n' || (y == ctx->vt->buf_y && x == ctx->vt->buf_x)) {
				break;
			}
			echo_char(ctx, ctx->vt->lines[y][x].unicode, x, y, data2);
		}
	}
	
	echo_char(ctx, '_', ctx->vt->buf_x, ctx->vt->buf_y, data2);

	magma_backend_put_buffer(backend, &buf);
}

void key_cb(magma_backend_t *backend, char *utf8, int length, void *data){
	((void)backend);
	magma_ctx_t *ctx = data;
	magma_log_info("Got to Key callback %d\n", length);	
	write(ctx->vt->master, utf8, length);

}

void resize_cb(magma_backend_t *backend, uint32_t height, uint32_t width, void *data) {
	UNUSED(backend);
	magma_ctx_t *ctx = data;
	struct winsize ws;

	ws.ws_xpixel = width;
	ws.ws_ypixel = height;
	ws.ws_col = width / (ctx->font->advance.x);
	ws.ws_row = height / (ctx->font->height);

	ctx->vt->lines = realloc(ctx->vt->lines, sizeof(void*) * ws.ws_row);


	if(ctx->vt->rows < ws.ws_row) {
		/*We grew*/
		for(int i = 0; i < ctx->vt->rows; i++) {
			ctx->vt->lines[i] = realloc(ctx->vt->lines[i], ws.ws_col * sizeof(glyph_t));
		}

		for(int i = ctx->vt->rows; i < ws.ws_row; i++) {
			ctx->vt->lines[i] = calloc(sizeof(glyph_t), ws.ws_col);
		}
	} else {
		/*we shrunk*/
		for(int i = 0; i < ws.ws_row; i++) {
			ctx->vt->lines[i] = realloc(ctx->vt->lines[i], sizeof(glyph_t) * ws.ws_col);
		}
	}

	ctx->vt->rows = ws.ws_row;
	ctx->vt->cols = ws.ws_col;
	ctx->width = width;
	ctx->height = height;
	if(ctx->vt->buf_y > ws.ws_row) {
		ctx->vt->buf_y = ws.ws_row;
		ctx->vt->buf_x = 0;
	}
	
	if(ioctl(ctx->vt->master, TIOCSWINSZ, &ws)) {
		printf("Failed to update term size\n");
	}
}

void on_close(magma_backend_t *backend, void *data) {
	UNUSED(backend);
	magma_ctx_t *ctx = data;

	ctx->is_running = 0;
}

int main(int argc, char **argv) {
	UNUSED(argc);
	UNUSED(argv);
	int slave;
	magma_ctx_t ctx = { 0 };
	struct pollfd pfd;
	magma_log_set_level(MAGMA_DEBUG);

	ctx.vt = calloc(1, sizeof(*ctx.vt));
	ctx.vt->lines = calloc(sizeof(void *), 25);
	for(int i = 0; i < 25; i++) {
		ctx.vt->lines[i] = calloc(sizeof(glyph_t), 80);
	}
	ctx.vt->cols = 80;
	ctx.vt->rows = 25;
	ctx.vt->fg = 0xf8f8f2;	

	if(magma_get_pty(&ctx.vt->master, &slave) < 0) {
		return -1;
	}

	ctx.width = 0;
	ctx.height = 0;
	ctx.is_running = 1;	

	FcInit();
	ctx.font = magma_font_init("monospace");
	FT_Set_Pixel_Sizes(ctx.font->face, 18, 18);
	ctx.font->height = ctx.font->face->size->metrics.height >> 6;
	
	/* Get the size of the M character to use as the advance width 
	 * as it will improve readableblity in Non monospace fonts 
	 * and NotoSanMono where the max advance is different
	 * as some glyphs in that font have different widths and thus
	 * advances based on this idea
	 * https://codeberg.org/dnkl/foot/commit/bb948d03e199870da6b35ba6f88ea88be12cfe21
	 */
	FT_Load_Char(ctx.font->face, 'M', FT_LOAD_DEFAULT);
	ctx.font->advance.x = ctx.font->face->glyph->advance.x >> 6;
	ctx.font->ascent = ctx.font->face->size->metrics.ascender >> 6;
	ctx.font->descent = ctx.font->face->size->metrics.descender >> 6;
	
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
	
		while(poll(&pfd, 1, 10)) {
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
	magma_font_deinit(ctx.font);
	FcFini();
	return 0;
}
