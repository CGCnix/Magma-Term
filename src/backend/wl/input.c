#include <wayland-client-protocol.h>
#include <wayland-client.h>

#include <magma/logger/log.h>
#include <magma/private/backend/wl.h>

#include <stdint.h>
#include <stdlib.h>

#include <sys/mman.h>
#include <xkbcommon/xkbcommon.h>

#define UNUSED(x) ((void)x)
/*
 * Wl_keyboard
 */

static void wl_keyboard_keymap(void *data, struct wl_keyboard *keyboard, uint32_t keymap_id, int32_t keymap_fd, uint32_t size) {
	UNUSED(keyboard);
	magma_wl_backend_t *wl;
	char *keymap_str;
	magma_log_debug("Kemap event: %d %d %d\n", keymap_id, keymap_fd, size);

	wl = data;

	keymap_str = mmap(NULL, size, PROT_READ, MAP_SHARED, keymap_fd, 0);


	wl->xkb_keymap = xkb_keymap_new_from_buffer(wl->xkb_context, keymap_str, size - 1, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
	
	wl->xkb_state = xkb_state_new(wl->xkb_keymap);
}

static void wl_keyboard_enter(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface, struct wl_array *keys) {
	UNUSED(data);
	UNUSED(keyboard);
	UNUSED(serial);
	UNUSED(keys);
	UNUSED(surface);
}

static void wl_keyboard_leave(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface) {
	UNUSED(data);
	UNUSED(keyboard);
	UNUSED(serial);
	UNUSED(surface);
}

static void wl_keyboard_key(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state) {
	magma_wl_backend_t *wl;
	char *buffer;
	size_t size;
	magma_log_debug("Key Event: %d(%d)\n", key, state);
	UNUSED(time);
	UNUSED(serial);
	UNUSED(keyboard);
	wl = data;


	size = xkb_state_key_get_utf8(wl->xkb_state, key + 8, NULL, 0) + 1;

	buffer = calloc(1, size);
	
	xkb_state_key_get_utf8(wl->xkb_state, key + 8, buffer, size);

	if(state) {
		wl->impl.key_press(data, buffer, size - 1, wl->impl.key_data);
	}
	free(buffer);	
}

static void wl_keyboard_mods(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group) {
	UNUSED(data);
	UNUSED(keyboard);
	UNUSED(serial);
	UNUSED(mods_locked);
	UNUSED(mods_latched);
	UNUSED(mods_depressed);
	UNUSED(group);
}

static void wl_keyboard_repeat_info(void *data, struct wl_keyboard *keyboard, int32_t rate, int32_t delay) {
	UNUSED(data);
	UNUSED(keyboard);
	UNUSED(rate);
	UNUSED(delay);
}

static const struct wl_keyboard_listener wl_keyboard_listener = {
	.keymap = wl_keyboard_keymap,
	.enter = wl_keyboard_enter,
	.leave = wl_keyboard_leave,
	.key = wl_keyboard_key,
	.modifiers = wl_keyboard_mods,
	.repeat_info = wl_keyboard_repeat_info,
};

/* 
 * Wl_pointer Functions
 */

/*TODO: FUNCTIONS*/

static const struct wl_pointer_listener wl_pointer_listener = {
	0	
};

/*
 * wl_touch functions
 */

/*TODO: FUNCTIONS*/


static const struct wl_touch_listener wl_touch_listener = {
	0	
};


void wl_seat_name(void *data, struct wl_seat *seat, const char *name) {
	magma_log_info("Seat name: %s\n", name);	
	UNUSED(seat);
	UNUSED(data);
}


void wl_seat_capabilities(void *data, struct wl_seat *seat, uint32_t capabilities) {
	magma_wl_backend_t *wl = data;
	magma_log_info("wl_seat caps: ");

	if(capabilities & WL_SEAT_CAPABILITY_KEYBOARD) {
		magma_log_printf(MAGMA_INFO, "WL_KEYBOARD ");
		wl->keyboard = wl_seat_get_keyboard(seat);
		wl_keyboard_add_listener(wl->keyboard, &wl_keyboard_listener, data);
	}

	if(capabilities & WL_SEAT_CAPABILITY_POINTER) {
		magma_log_printf(MAGMA_INFO, "| WL_POINTER ");
		UNUSED(wl_touch_listener);
	}

	if(capabilities & WL_SEAT_CAPABILITY_TOUCH) {
		magma_log_printf(MAGMA_INFO, "| WL_TOUCH");
		UNUSED(wl_pointer_listener);
	}

	magma_log_printf(MAGMA_INFO, "\n");
	
}
