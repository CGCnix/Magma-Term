#include <pty.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

#include <poll.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <magma/logger/log.h>
#include <magma/backend/backend.h>

typedef struct pty {
	int master, slave;
} pty_t;

typedef struct magmatty {
	pty_t pty;

	unsigned char buf[2560];
	int use;

	magma_backend_t *backend;

	FT_Library library;
	FT_Face face;

	uint32_t width, height, xpos, ypos;
} magmatty_t;

int pty_get_master_slave(int *pmaster, int *pslave) {
	char *slave_path;

	*pmaster = posix_openpt(O_RDWR | O_NOCTTY);
	if(*pmaster < 0) {
		printf("posix_openpt: %m\n");
		return -1;
	}

	if(grantpt(*pmaster) < 0) {
		close(*pmaster);
		printf("grantpt: %m\n");
		return -1;
	}

	if(unlockpt(*pmaster) < 0) {
		close(*pmaster);
		printf("unlockpt: %m\n");
		return -1;
	}

	slave_path = ptsname(*pmaster);
	if(slave_path == NULL)  {
		close(*pmaster);
		printf("ptsname: %m\n");
		return -1;
	}
	printf("PTSNAME: %s\n", slave_path);

	*pslave = open(slave_path, O_RDWR | O_NOCTTY);
	if(*pslave < 0) {
		close(*pmaster);
		printf("open(%s): %m\n", slave_path);
		return -1;
	}

	return *pmaster;
};

pid_t fork_pty_pair(int master, int slave) {
	pid_t pid;

	const char *argv[] = { "/bin/sh", NULL };
	const char *envp[] = { "TERM=dumb", NULL };
	
	pid = fork();
	if(pid == 0) {
		close(master);


		setsid();
		if(ioctl(slave, TIOCSCTTY, NULL) == -1) {
			printf("IOCTL:(TIOCSCTTY) %m\n");
			return -1;
		}

		/*Dup slave as stdin, stdout, stderr*/
		for(int i = 0; i < 3; i++) {
			dup2(slave, i);
		}
		
		execve(argv[0], (char**)argv, (char**)envp);
		return -1;
	} else if(pid > 0) {
		close(slave);
		return pid;
	}

	printf("fork: %m\n");
	return -1;
}

static int xpos = 0;
static int ypos = 0;
void echo_char(magmatty_t *ctx, int ch, uint32_t width, unsigned int maxAscent, uint32_t *data2) {
	FT_Bitmap *bitmap;
	if(ch == '\r') {
		return;
	}
	if(ch == '\n') {
		ypos += 13;
		xpos = 0;
		return;
	} 
	if(ch == 0x9) {
		return;
	}
	FT_UInt glyphindex = FT_Get_Char_Index(ctx->face, ch);


	FT_Load_Glyph(ctx->face, glyphindex, FT_LOAD_DEFAULT);
	FT_Render_Glyph(ctx->face->glyph, FT_RENDER_MODE_NORMAL);
	
	bitmap = &ctx->face->glyph->bitmap;

	for(int y = 0; y < bitmap->rows; y++) {
		for(int x = 0; x < bitmap->width; x++) {
			if(bitmap->buffer[y * bitmap->pitch + x]) {
				if(xpos + 32 >= width) {
					ypos += 13;
					xpos = 0;
				}
				data2[(20 + y + ypos + (maxAscent - ctx->face->glyph->bitmap_top)) * width + x + xpos] = bitmap->buffer[y * bitmap->width + x];

			}
		}
	}
	xpos += 13;

}

uint16_t utf8_to_utf16(uint16_t b1, uint16_t b2, uint16_t b3) {
	if((b1 & 0xe0) == 0xc0) {
		return ((b1 & 0x1f) << 6) | (b2 & 0x3f);
	}
	return ((b1 & 0x0f) << 12) | ((b2 & 0x3f) << 6) | b3 & 0x3f;
}

void draw_cb(magma_backend_t *backend, uint32_t height, uint32_t width, void *data) {
	if(width == 0) return;
	magmatty_t *ctx = data;
	uint32_t *data2;
	magma_buf_t buf = {
		.width = width,
		.height = height,
		.bpp = 32,
		.depth = 24,
		.buffer = NULL,
	};

	xpos = 0;
	ypos = 0;

	buf.buffer = calloc(4, width * height);
	data2 = buf.buffer;
	for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
			data2[y * width + x] = 0xFF000000;
        }
    }

	unsigned int maxascent = 0;
	unsigned int maxDescent = 0;
	FT_GlyphSlot glyph;

	ctx->height = height;
	ctx->width = width;
	for(int i = 0; i < ctx->use; i++) {
		FT_UInt glyphindex = FT_Get_Char_Index(ctx->face, ctx->buf[i]);
		FT_Load_Glyph(ctx->face, glyphindex, FT_LOAD_DEFAULT);
		FT_Render_Glyph(ctx->face->glyph, FT_RENDER_MODE_NORMAL);
		glyph = ctx->face->glyph;

		if(maxascent < ctx->face->glyph->bitmap_top) {
			maxascent = ctx->face->glyph->bitmap_top;
		}
		if ((glyph->metrics.height >> 6) - glyph->bitmap_top > (int)maxDescent) {
    		maxDescent = (glyph->metrics.height >> 6) - glyph->bitmap_top;
		}
	}
	for(int i = 0; i < ctx->use; i++) {
		if((ctx->buf[i] & 0xf0) == 0xe0) {
			uint16_t utf16 = utf8_to_utf16(ctx->buf[i], ctx->buf[i + 1], ctx->buf[i + 2]);
			echo_char(ctx, utf16, width, maxascent, data2);
			i++;
			i++;
		} else if((ctx->buf[i] & 0xe0) == 0xc0) {
			uint16_t utf16 = utf8_to_utf16(ctx->buf[i], ctx->buf[i + 1], 0);
			echo_char(ctx, utf16, width, maxascent, data2);
			i++;
		} else {
			echo_char(ctx, (ctx->buf[i]), width, maxascent, data2);
		}
	}
	magma_backend_put_buffer(backend, &buf);
}

static int run = 1;
void key_cb(magma_backend_t *backend, char *utf8, int length, void *data){
	magmatty_t *ctx = data;
	
	if(utf8[0] == 0x08) {
		write(ctx->pty.master, "\177", 1);
	} else {
		write(ctx->pty.master, utf8, length);
	}
}

void resize_cb(magma_backend_t *backend, uint32_t height, uint32_t width, void *data) {
	magmatty_t *ctx = data;
	struct winsize ws;
	ws.ws_xpixel = width;
	ws.ws_ypixel = height;
	ws.ws_col = width / 13;
	ws.ws_row = height / 13;

	printf("COL: %d ROW: %d\n", ws.ws_col, ws.ws_row);

	if(ioctl(ctx->pty.master, TIOCSWINSZ, &ws)) {
		printf("Failed to update term size\n");
	}
}

int main(int argc, char **argv) {
	magmatty_t ctx = { 0 };
	struct pollfd pfd;

	if(pty_get_master_slave(&ctx.pty.master, &ctx.pty.slave) < 0) {
		return -1;
	}

	ctx.width = 0;
	ctx.height = 0;
	FT_Init_FreeType(&ctx.library);
	FT_New_Face(ctx.library, "/usr/share/fonts/NerdFonts/ttf/CousineNerdFontMono-Regular.ttf", 0, &ctx.face);

	FT_Set_Pixel_Sizes(ctx.face, 13, 13);

	fork_pty_pair(ctx.pty.master, ctx.pty.slave);

	ctx.backend = magma_backend_init_auto();

	magma_backend_set_on_resize(ctx.backend, resize_cb, &ctx);
	magma_backend_set_on_draw(ctx.backend, draw_cb, &ctx);
	magma_backend_set_on_key(ctx.backend, key_cb, &ctx);

	magma_backend_start(ctx.backend);

	pfd.fd = ctx.pty.master;
	pfd.events = POLLIN;
	write(ctx.pty.master, "ls\n", 3);
	while(run) {
		magma_backend_dispatch_events(ctx.backend);
	
		if(poll(&pfd, 1, 10)) {
			if(pfd.revents & POLLERR || pfd.revents & POLLHUP) {
				printf("Child is process has closed\n");
				return 1;
			}

			read(ctx.pty.master, &ctx.buf[ctx.use], 1);
			if (ctx.buf[ctx.use] != 0x8) {
				ctx.use++;
			} else {
				ctx.use--;
			}
		}
			draw_cb(ctx.backend, ctx.height, ctx.width, &ctx);
	}

	magma_backend_deinit(ctx.backend);

	return 0;
}
