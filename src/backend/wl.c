/*Wayland Libraries*/
#include <poll.h>
#include <stddef.h>
#include <string.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>

#include <xdg-shell.h>

/*xkbcommon*/
#include <xkbcommon/xkbcommon.h>

/*Std C*/
#include <stdlib.h>
#include <stdint.h>

/*linux headers*/
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>

/*Magma Headers*/
#include <magma/private/backend/backend.h>
#include <magma/backend/backend.h>
#include <magma/logger/log.h>


typedef struct magma_wl_backend {
	magma_backend_t impl;

	struct wl_display *display;
	struct wl_registry *registry;

	struct wl_compositor *compositor;
	struct wl_surface *surface;
	struct wl_shm *shm;

	struct wl_seat *seat;
	struct wl_keyboard *keyboard;
	struct wl_pointer *pointer;

	struct xdg_wm_base *xdg_wm_base;
	struct xdg_surface *xdg_surface;
	struct xdg_toplevel *xdg_toplevel;

	struct xkb_context *xkb_context;
	struct xkb_keymap *xkb_keymap;
	struct xkb_state *xkb_state;


	uint32_t width, height;
} magma_wl_backend_t;

void wl_keyboard_keymap(void *data, struct wl_keyboard *keyboard, uint32_t keymap_id, int32_t keymap_fd, uint32_t size) {
	magma_wl_backend_t *wl;
	char *keymap_str;
	magma_log_debug("Kemap event: %d %d %d\n", keymap_id, keymap_fd, size);

	wl = data;

	keymap_str = mmap(NULL, size, PROT_READ, MAP_SHARED, keymap_fd, 0);


	wl->xkb_keymap = xkb_keymap_new_from_buffer(wl->xkb_context, keymap_str, size - 1, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
	
	wl->xkb_state = xkb_state_new(wl->xkb_keymap);

}

void wl_keyboard_enter(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface, struct wl_array *keys) {

}

void wl_keyboard_leave(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface) {

}

void wl_keyboard_key(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state) {
	magma_wl_backend_t *wl;
	char *buffer;
	size_t size;
	magma_log_debug("Key Event: %d(%d)\n", key, state);
	
	wl = data;


	size = xkb_state_key_get_utf8(wl->xkb_state, key + 8, NULL, 0) + 1;

	buffer = calloc(1, size);
	
	xkb_state_key_get_utf8(wl->xkb_state, key + 8, buffer, size);

	for(int i = 0; i < size; i++) {
		printf("%d, ", buffer[i]);
	}
	printf("\n");

	if(state) {
		wl->impl.key_press(data, buffer, size - 1, wl->impl.key_data);
	}
	
}

void wl_keyboard_mods(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group) {
	
}

void wl_keyboard_repeat_info(void *data, struct wl_keyboard *keyboard, int32_t rate, int32_t delay) {

}

const struct wl_keyboard_listener wl_keyboard_listener = {
	.keymap = wl_keyboard_keymap,
	.enter = wl_keyboard_enter,
	.leave = wl_keyboard_leave,
	.key = wl_keyboard_key,
	.modifiers = wl_keyboard_mods,
	.repeat_info = wl_keyboard_repeat_info,
};

void wl_seat_name(void *data, struct wl_seat *seat, const char *name) {
	magma_log_info("Seat name: %s\n", name);	
}

void wl_seat_capabilities(void *data, struct wl_seat *seat, uint32_t capabilities) {
	magma_wl_backend_t *wl = data;
	magma_log_info("Seat0 caps: %d\n", capabilities);

	if(capabilities & WL_SEAT_CAPABILITY_KEYBOARD) {
		wl->keyboard = wl_seat_get_keyboard(seat);
		wl_keyboard_add_listener(wl->keyboard, &wl_keyboard_listener, data);
	}
}

const struct wl_seat_listener wl_seat_listener = {
	.name = wl_seat_name,
	.capabilities = wl_seat_capabilities,	
};

void xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial) {
	xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
	.ping = xdg_wm_base_ping,
};

void wl_registry_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
	magma_wl_backend_t *wl;
	magma_log_info("WL_GLOBAL: %s(%d)\n", interface, version);

	wl = data;

	if(strcmp(interface, wl_compositor_interface.name) == 0) {
		wl->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, version);
	} else if(strcmp(interface, wl_shm_interface.name) == 0) {
		wl->shm = wl_registry_bind(registry, name, &wl_shm_interface, version);
	} else if(strcmp(interface, wl_seat_interface.name) == 0) {
		wl->seat = wl_registry_bind(registry, name, &wl_seat_interface, version);
		wl_seat_add_listener(wl->seat, &wl_seat_listener, data);
	} else if(strcmp(interface, xdg_wm_base_interface.name) == 0) {
		wl->xdg_wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, version);
		xdg_wm_base_add_listener(wl->xdg_wm_base, &xdg_wm_base_listener, NULL);
	}
}

void wl_registry_global_remove(void *data, struct wl_registry *registry, uint32_t name) {

}

static const struct wl_registry_listener registry_listener = {
	.global = wl_registry_global,
	.global_remove = wl_registry_global_remove,
};

static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial) {
	magma_wl_backend_t *wl = data;
	
	magma_log_info("xdg_surface_configure serial: %d\n", serial);
	
	xdg_surface_ack_configure(xdg_surface, serial);

	wl->impl.draw((void*)wl, wl->height, wl->width, wl->impl.draw_data);

}

void xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel) {
	magma_log_warn("We should close %p\n", xdg_toplevel);
}

void xdg_toplevel_wm_capabilities(void *data, struct xdg_toplevel *xdg_toplevel, struct wl_array *caps) {

}

void xdg_toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height, struct wl_array *states) {
	magma_wl_backend_t *wl = data;

	wl->height = height ? : 600;
	wl->width = width ? : 600;
}

void xdg_toplevel_configure_bounds(void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height) {

}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
	.close = xdg_toplevel_close,
	.wm_capabilities = xdg_toplevel_wm_capabilities,
	.configure = xdg_toplevel_configure,
	.configure_bounds = xdg_toplevel_configure_bounds,
};

static const struct xdg_surface_listener xdg_surface_listener = {
	.configure = xdg_surface_configure,
};

void magma_wl_backend_dispatch(magma_backend_t *backend) {
	magma_wl_backend_t *wl = (void*)backend;
	wl_display_dispatch(wl->display);
}

static void
randname(char *buf)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    long r = ts.tv_nsec;
    for (int i = 0; i < 6; ++i) {
        buf[i] = 'A'+(r&15)+(r&16)*2;
        r >>= 5;
    }
}

static int
create_shm_file(void)
{
    int retries = 100;
    do {
        char name[] = "/wl_shm-XXXXXX";
        randname(name + sizeof(name) - 7);
        --retries;
        int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
        if (fd >= 0) {
            shm_unlink(name);
            return fd;
        }
    } while (retries > 0 && errno == EEXIST);
    return -1;
}

static int
allocate_shm_file(size_t size)
{
    int fd = create_shm_file();
    if (fd < 0)
        return -1;
    int ret;
    do {
        ret = ftruncate(fd, size);
    } while (ret < 0 && errno == EINTR);
    if (ret < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

void magma_wl_backend_put_buffer(magma_backend_t *backend, magma_buf_t *buffer) {
	magma_wl_backend_t *wl = (void*)backend;
	int fd = allocate_shm_file(buffer->width * buffer->height * 4);


	void *data = mmap(NULL, buffer->width * buffer->height * 4, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	struct wl_shm_pool *pool = wl_shm_create_pool(wl->shm, fd, buffer->width * buffer->height * 4);
	struct wl_buffer *buf = wl_shm_pool_create_buffer(pool, 0, buffer->width, buffer->height, buffer->width * 4, WL_SHM_FORMAT_ARGB8888);
	wl_shm_pool_destroy(pool);
	close(fd);

	memcpy(data, buffer->buffer, buffer->width * buffer->height * 4);

	munmap(data, buffer->width * buffer->height * 4);

	free(buffer->buffer);
	wl_surface_damage(wl->surface, 0, 0, buffer->width, buffer->height);
	wl_surface_attach(wl->surface, buf, 0, 0);
	wl_surface_commit(wl->surface);
}

void magma_wl_backend_start(magma_backend_t *backend) {
	magma_wl_backend_t *wl = (void*)backend;

	wl_surface_commit(wl->surface);
}

magma_backend_t *magma_wl_backend_init() {
	magma_wl_backend_t *wl;

	wl = calloc(1, sizeof(magma_wl_backend_t));

	wl->display = wl_display_connect(NULL);

	wl->registry = wl_display_get_registry(wl->display);
	wl_registry_add_listener(wl->registry, &registry_listener, wl);

	wl_display_roundtrip(wl->display);

	wl->surface = wl_compositor_create_surface(wl->compositor);

	wl->xdg_surface = xdg_wm_base_get_xdg_surface(wl->xdg_wm_base, wl->surface);
	xdg_surface_add_listener(wl->xdg_surface, &xdg_surface_listener, wl);


	wl->xdg_toplevel = xdg_surface_get_toplevel(wl->xdg_surface);
	xdg_toplevel_add_listener(wl->xdg_toplevel, &xdg_toplevel_listener, wl);

	wl->xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

	wl->impl.start = magma_wl_backend_start;
	wl->impl.dispatch_events = magma_wl_backend_dispatch;
	wl->impl.put_buffer = magma_wl_backend_put_buffer;
	


	return (void*)wl;
}
