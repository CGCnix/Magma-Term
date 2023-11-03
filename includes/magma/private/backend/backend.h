#pragma once

#include <stdint.h>
#include <magma/backend/backend.h>

struct magma_backend {
	void (*start)(magma_backend_t *backend);
	void (*deinit)(magma_backend_t *backend);
	void (*dispatch_events)(magma_backend_t *backend);


	/*resize*/
	void (*draw)(magma_backend_t *backend, uint32_t height, uint32_t width, void *data);
	void (*resize)(magma_backend_t *backend, uint32_t height, uint32_t width, void *data);
	
	/*TODO: IMPLEMENT*/
	void (*button_press)(magma_backend_t *backend);
	void (*key_press)(magma_backend_t *backend, char *utf8, int length, void *data);
	void (*enter)(magma_backend_t *backend);
	void (*cursor_motion)(magma_backend_t *backends);
	void (*put_buffer)(magma_backend_t *backend, magma_buf_t *buffer);

	void *draw_data;
	void *resize_data;
	void *button_data;
	void *key_data;
	void *enter_data;
	void *cursor_data;
};
