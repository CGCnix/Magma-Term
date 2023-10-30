#pragma once

#include <stdint.h>

typedef struct magma_backend magma_backend_t;

/* Buffers Auto assume RGB FORMAT
 * and should be used a fallback when no
 * rendering through hardware is avliable
 */
typedef struct magma_buf {
	uint32_t width, height;
	uint8_t depth, bpp;
	void *buffer;
}magma_buf_t;

magma_backend_t *magma_backend_init_name(const char *name);
magma_backend_t *magma_backend_init_auto();
void magma_backend_start(magma_backend_t *backend);
void magma_backend_deinit(magma_backend_t *backend);
void magma_backend_dispatch_events(magma_backend_t *backend);

void magma_backend_set_on_draw(magma_backend_t *backend, void (*draw)(magma_backend_t *backend, uint32_t height, uint32_t width, void *data), void *data);
void magma_backend_set_on_resize(magma_backend_t *backend, void (*resize)(magma_backend_t *backend, uint32_t height, uint32_t width, void *data), void *data);
void magma_backend_set_on_button(magma_backend_t *backend, void (*button_press)(magma_backend_t *backend), void *data);
void magma_backend_set_on_key(magma_backend_t *backend, void (*key_press)(magma_backend_t *backend, int key, void *data), void *data);
void magma_backend_set_on_enter(magma_backend_t *backend, void (*enter)(magma_backend_t *backend), void *data);
void magma_backend_set_on_cursor(magma_backend_t *backend, void (*cursor_motion)(magma_backend_t *backends), void *data);
void magma_backend_put_buffer(magma_backend_t *backend, magma_buf_t *buffer);
