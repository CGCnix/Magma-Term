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
#include <unistd.h>

/*Magma Headers*/
#include <magma/private/backend/backend.h>
#include <magma/private/backend/wl.h>
#include <magma/backend/backend.h>
#include <magma/logger/log.h>

#define UNUSED(x) ((void)x)

const struct wl_seat_listener wl_seat_listener = {
	.name = wl_seat_name,
	.capabilities = wl_seat_capabilities,	
};

void xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial) {
	xdg_wm_base_pong(xdg_wm_base, serial);
	UNUSED(data);
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
	UNUSED(data);
	UNUSED(registry);
	UNUSED(name);
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

static void xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel) {
	magma_wl_backend_t *wl = data;
	magma_log_warn("We should close %p\n", xdg_toplevel);
	

	if(wl->impl.close) {
		wl->impl.close(data, wl->impl.close_data);
	}

}

static void xdg_toplevel_wm_capabilities(void *data, struct xdg_toplevel *xdg_toplevel, struct wl_array *caps) {
	magma_log_info("xdg_toplevel_wm_caps\n");
	UNUSED(data);
	UNUSED(xdg_toplevel);
	UNUSED(caps);
}

static void xdg_toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height, struct wl_array *states) {
	magma_wl_backend_t *wl = data;

	wl->height = height ? height : 600;
	wl->width = width ? width : 600;
	if(wl->impl.resize) {
		wl->impl.resize(data, wl->height, wl->width, wl->impl.resize_data);
	}
	UNUSED(states);
	UNUSED(xdg_toplevel);
}

static void xdg_toplevel_configure_bounds(void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height) {
	UNUSED(data);
	UNUSED(xdg_toplevel);
	UNUSED(width);
	UNUSED(height);
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

	/*Is there a way to do this in a non blocking way?*/
		wl_display_dispatch(wl->display); 
}

static int allocate_shm_fd(size_t size) {
	int fd, ret;
	char name[] = "/tmp/magma_wl_shm-XXXXXX";

	fd = mkstemp(name);
	if(fd < 0) {
		return -1;
	}
	/*Unlink the name from this fd*/
	unlink(name);
	
	ret = ftruncate(fd, size);
	if(ret < 0) {
		close(fd);
		fd = -1;
	}	

	return fd;
}

void wl_buffer_release(void *data, struct wl_buffer *buffer) {
	wl_buffer_destroy(buffer);
	UNUSED(data);
}

static const struct wl_buffer_listener wl_buffer_listener = {
	.release = wl_buffer_release,
};

void magma_wl_backend_put_buffer(magma_backend_t *backend, magma_buf_t *buffer) {
	magma_wl_backend_t *wl = (void*)backend;
	int fd = allocate_shm_fd(buffer->width * buffer->height * 4);


	void *data = mmap(NULL, buffer->width * buffer->height * 4, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	struct wl_shm_pool *pool = wl_shm_create_pool(wl->shm, fd, buffer->width * buffer->height * 4);
	struct wl_buffer *buf = wl_shm_pool_create_buffer(pool, 0, buffer->width, buffer->height, buffer->width * 4, WL_SHM_FORMAT_ARGB8888);
	wl_shm_pool_destroy(pool);
	close(fd);

	wl_buffer_add_listener(buf, &wl_buffer_listener, NULL);

	memcpy(data, buffer->buffer, buffer->width * buffer->height * 4);

	munmap(data, buffer->width * buffer->height * 4);

	free(buffer->buffer);
	wl_surface_damage_buffer(wl->surface, 0, 0, buffer->width, buffer->height);

	wl_surface_attach(wl->surface, buf, 0, 0);
	wl_surface_commit(wl->surface);
}

void magma_wl_backend_start(magma_backend_t *backend) {
	magma_wl_backend_t *wl = (void*)backend;

	wl_surface_commit(wl->surface);
	wl_display_flush(wl->display);
}

void magma_wl_backend_deinit(magma_backend_t *backend) {
	magma_wl_backend_t *wl = (void*)backend;

	xdg_toplevel_destroy(wl->xdg_toplevel);
	
	xdg_surface_destroy(wl->xdg_surface);

	xdg_wm_base_destroy(wl->xdg_wm_base);

	wl_surface_destroy(wl->surface);

	wl_compositor_destroy(wl->compositor);

	wl_keyboard_destroy(wl->keyboard);

	wl_seat_destroy(wl->seat);
	
	wl_shm_destroy(wl->shm);

	wl_registry_destroy(wl->registry);

	wl_display_disconnect(wl->display);

	free(wl);
	
}

magma_backend_t *magma_wl_backend_init(void) {
	magma_wl_backend_t *wl;

	wl = calloc(1, sizeof(magma_wl_backend_t));

	wl->display = wl_display_connect(NULL);
	wl->display_fd = wl_display_get_fd(wl->display);

	wl->registry = wl_display_get_registry(wl->display);
	wl_registry_add_listener(wl->registry, &registry_listener, wl);

	wl_display_roundtrip(wl->display);

	wl->surface = wl_compositor_create_surface(wl->compositor);

	wl->xdg_surface = xdg_wm_base_get_xdg_surface(wl->xdg_wm_base, wl->surface);
	xdg_surface_add_listener(wl->xdg_surface, &xdg_surface_listener, wl);


	wl->xdg_toplevel = xdg_surface_get_toplevel(wl->xdg_surface);
	xdg_toplevel_add_listener(wl->xdg_toplevel, &xdg_toplevel_listener, wl);

	wl->impl.start = magma_wl_backend_start;
	wl->impl.dispatch_events = magma_wl_backend_dispatch;
	wl->impl.put_buffer = magma_wl_backend_put_buffer;
	wl->impl.deinit = magma_wl_backend_deinit;
	wl->impl.get_state = magma_wl_backend_get_xkbstate;
	wl->impl.get_kmap = magma_wl_backend_get_xkbmap;

	return (void*)wl;
}
