#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <unistd.h>

#include <magma/backend/backend.h>
#include <magma/private/backend/backend.h>
#include <magma/logger/log.h>

#include <magma/backend/xcb.h>
#include <magma/backend/drm.h>
#include <magma/backend/wl.h>


magma_backend_t *magma_backend_init_name(const char *name) {
	if(strcmp(name, "xcb") == 0) {
		return magma_xcb_backend_init();
	} else if(strcmp(name, "wayland") == 0) {
		return magma_wl_backend_init();
	} else if(strcmp(name, "drm") == 0) {
		return magma_drm_backend_init();
	} 

	magma_log_error("Backend: %s not present\n", name);
	return NULL;
}

magma_backend_t *magma_backend_init_auto() {
	char *override;

	/*Allow overriding so I can test Xwindow from wayland*/
	override = getenv("MAGMA_BACKEND");

	if(override) {
		return magma_backend_init_name(override);
	}

	/* In theroy if this is set a wayland
	 * compositor should be running
	 */
	if(getenv("WAYLAND_DISPLAY")) {
		return magma_wl_backend_init();
	}
	
	/* If this is set either X server or 
	 * Xwayland server shoudle be running
	 */
	if(getenv("DISPLAY")) {
		return magma_xcb_backend_init();
	}

	return magma_drm_backend_init();
}

void magma_backend_start(magma_backend_t *backend) {
	backend->start(backend);
}

void magma_backend_dispatch_events(magma_backend_t *backend) {
	backend->dispatch_events(backend);
}

void magma_backend_deinit(magma_backend_t *backend) {
	backend->deinit(backend);
}

void magma_backend_set_on_draw(magma_backend_t *backend, PFN_MAGMADRAWCB draw, void *data) {
	backend->draw = draw;
	backend->draw_data = data;
}

void magma_backend_set_on_resize(magma_backend_t *backend, PFN_MAGMARESIZCB resize, void *data) {
	backend->resize = resize;
	backend->resize_data = data;
}

void magma_backend_set_on_close(magma_backend_t *backend, PFN_MAGMACLOSECB closefn, void *data) {
	backend->close = closefn;
	backend->close_data = data;
}

void magma_backend_set_on_button(magma_backend_t *backend, void (*button_press)(magma_backend_t *backend), void *data) {
	backend->button_press = button_press;
	backend->button_data = data;
}

void magma_backend_set_on_key(magma_backend_t *backend, void (*key_press)(magma_backend_t *backend, char *utf8, int length, void *data), void *data) {
	backend->key_press = key_press;
	backend->key_data = data;
}

void magma_backend_set_on_enter(magma_backend_t *backend, void (*enter)(magma_backend_t *backend), void *data) {
	backend->enter = enter;
	backend->enter_data = data;
}

void magma_backend_set_on_cursor(magma_backend_t *backend, void (*cursor_motion)(magma_backend_t *backends), void *data) {
	backend->cursor_motion = cursor_motion;
	backend->cursor_data = data;
}

void magma_backend_put_buffer(magma_backend_t *backend, magma_buf_t *buffer) {
	backend->put_buffer(backend, buffer);
}
