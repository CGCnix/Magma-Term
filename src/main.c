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

#include <magma/backend/backend.h>

typedef struct pty {
	int master, slave;
} pty_t;

typedef struct magmatty {
	pty_t pty;

	char buf[2560];
	int use;

	magma_backend_t *backend;

	FT_Library library;
	FT_Face face;

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
	if(ch == '\n') {
		ypos +=16;
		xpos = 0;
		return;
	}
	FT_UInt glyphindex = FT_Get_Char_Index(ctx->face, ch);

	FT_Load_Glyph(ctx->face, glyphindex, FT_LOAD_DEFAULT);
	FT_Render_Glyph(ctx->face->glyph, FT_RENDER_MODE_NORMAL);
	
	bitmap = &ctx->face->glyph->bitmap;

	for(int y = 0; y < bitmap->rows; y++) {
		for(int x = 0; x < bitmap->width; x++) {
			if(bitmap->buffer[y * bitmap->pitch + x]) {
				if(xpos + 16 >= width) {
					ypos += 16;
					xpos = 0;
				}
				data2[(20 + y + ypos + (maxAscent - ctx->face->glyph->bitmap_top)) * width + 20 + x + xpos] = bitmap->buffer[y * bitmap->width + x];

			}
		}
	}
	xpos += 16;

}

uint32_t gwidth;
uint32_t gheight;

void draw_cb(magma_backend_t *backend, uint32_t height, uint32_t width, void *data) {
	magmatty_t *ctx = data;
	uint32_t *data2;
	FT_Bitmap *bitmap = &ctx->face->glyph->bitmap;
	magma_buf_t buf = {
		.width = width,
		.height = height,
		.bpp = 32,
		.depth = 24,
		.buffer = NULL,
	};

	xpos = 0;
	ypos = 0;

	buf.buffer = calloc(5, width * height);
	data2 = buf.buffer;
	for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
			data2[y * width + x] = 0xFF000000;
        }
    }

	unsigned int maxascent = 0;
	unsigned int maxDescent = 0;
	FT_GlyphSlot glyph;

	gheight = height;
	gwidth = width;

	for(int i = 0; i < ctx->use; i++) {
		FT_UInt glyphindex = FT_Get_Char_Index(ctx->face, ctx->buf[i]);
		FT_Load_Glyph(ctx->face, glyphindex, FT_LOAD_DEFAULT);
		FT_Render_Glyph(ctx->face->glyph, FT_RENDER_MODE_MONO);
		glyph = ctx->face->glyph;

		if(maxascent < ctx->face->glyph->bitmap_top) {
			maxascent = ctx->face->glyph->bitmap_top;
		}
		if ((glyph->metrics.height >> 6) - glyph->bitmap_top > (int)maxDescent) {
    		maxDescent = (glyph->metrics.height >> 6) - glyph->bitmap_top;
		}
	}

	for(int i = 0; i < ctx->use; i++) {
		echo_char(ctx, ctx->buf[i], width, maxascent, data2);
	}
	magma_backend_put_buffer(backend, &buf);
}

static int run = 1;
void key_cb(magma_backend_t *backend, int key, void *data){
	magmatty_t *ctx = data;
	if(key == 0x24) {
		write(ctx->pty.master, "\n", 1);
	}
	if(key == 0x2e) {
		write(ctx->pty.master, "l", 1);
	}
	if(key == 0x27) {
		write(ctx->pty.master, "s", 1);
	} 
	if(key == 0x1b) {
		write(ctx->pty.master, "r", 1);
	}
	if(key == 0x1a) {
		write(ctx->pty.master, "e", 1);
	} 
	if(key == 0x26) {
		write(ctx->pty.master, "a", 1);
	}
	if(key == 0x36) {
		write(ctx->pty.master, "c", 1);
	}

}

void resize_cb(magma_backend_t *backend, uint32_t height, uint32_t width, void *data) {
	
}

int main(int argc, char **argv) {
	magmatty_t ctx = { 0 };
	struct pollfd pfd;

	if(pty_get_master_slave(&ctx.pty.master, &ctx.pty.slave) < 0) {
		return -1;
	}
	gwidth = 600;
	gheight = 600;
	FT_Init_FreeType(&ctx.library);
	FT_New_Face(ctx.library, "/usr/share/fonts/ttf/JetBrainsMonoNL-Bold.ttf", 0, &ctx.face);

	FT_Set_Pixel_Sizes(ctx.face, 16, 16);

	fork_pty_pair(ctx.pty.master, ctx.pty.slave);

	ctx.backend = magma_backend_init_auto();
	magma_backend_start(ctx.backend);

	magma_backend_set_on_draw(ctx.backend, draw_cb, &ctx);
	magma_backend_set_on_key(ctx.backend, key_cb, &ctx);
	
	pfd.fd = ctx.pty.master;
	pfd.events = POLLIN;
	while(run) {
		magma_backend_dispatch_events(ctx.backend);
	
		if(poll(&pfd, 1, 10)) {
			read(ctx.pty.master, &ctx.buf[ctx.use], 1);
		}
		draw_cb(ctx.backend, gheight, gwidth, &ctx);
	}

	magma_backend_deinit(ctx.backend);

	return 0;
}
