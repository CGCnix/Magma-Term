#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <unistd.h>

#include <magma/backend/backend.h>
#include <magma/private/backend/backend.h>
#include <magma/logger/log.h>
#include <xkbcommon/xkbcommon.h>

#ifndef _MAGMA_NO_XCB_
	#include <magma/backend/xcb.h>
#endif 

#ifndef _MAGMA_NO_WL_
	#include <magma/backend/wl.h>
#endif

#ifndef _MAGMA_NO_DRM_

#include <magma/backend/drm.h>
#endif

magma_backend_t *magma_backend_init_name(const char *name) {
	#ifndef _MAGMA_NO_XCB_
	if(strcmp(name, "xcb") == 0) {
		return magma_xcb_backend_init();
	} 
	#endif 
	
	#ifndef _MAGMA_NO_WL_
	if(strcmp(name, "wayland") == 0) {
		return magma_wl_backend_init();
	}
	#endif

	#ifndef _MAGMA_NO_DRM_
	if(strcmp(name, "drm") == 0) {
		return magma_drm_backend_init();
	}
	#endif

	magma_log_error("Backend: %s not present\n", name);
	return NULL;
}

magma_backend_t *magma_backend_init_auto(void) {
	char *override;

	#if defined(_MAGMA_NO_WL_) && defined(_MAGMA_NO_XCB_) && defined(_MAGMA_NO_DRM_)
		#error "MAGMA COMPILED WITH NO BACKENDS\nplease remove one --disable-backend argumets"
	#endif

	/*Allow overriding so I can test Xwindow from wayland*/
	override = getenv("MAGMA_BACKEND");

	if(override) {
		return magma_backend_init_name(override);
	}

	/* In theroy if this is set a wayland
	 * compositor should be running
	 */
	#ifndef _MAGMA_NO_WL_
	if(getenv("WAYLAND_DISPLAY")) {
		return magma_wl_backend_init();
	}
	#endif
	/* If this is set either X server or 
	 * Xwayland server shoudle be running
	 */
	#ifndef _MAGMA_NO_XCB_
	if(getenv("DISPLAY")) {
		return magma_xcb_backend_init();
	}
	#endif

	#ifndef _MAGMA_NO_DRM_
	
	return magma_drm_backend_init();
	#else 
	return NULL;
	#endif
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

void magma_backend_set_on_key(magma_backend_t *backend, PFN_MAGMAKEYCB key_press, void *data) {
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

void magma_backend_set_on_keymap(magma_backend_t *backend, PFN_MAGMAKEYMAPCB keymap, void *data) {
	backend->keymap = keymap;
	backend->keymap_data = data;
}

struct xkb_keymap *magma_backend_get_xkbmap(magma_backend_t *backend, struct xkb_context *context) {
	return backend->get_kmap(backend, context);
}

struct xkb_state *magma_backend_get_xkbstate(magma_backend_t *backend, struct xkb_keymap *keymap) {
	return backend->get_state(backend, keymap);
}

void magma_backend_put_buffer(magma_backend_t *backend, magma_buf_t *buffer) {
	backend->put_buffer(backend, buffer);
}
