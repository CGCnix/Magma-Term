#pragma once

#include <stdint.h>
#include <wayland-client.h>

#include <magma/private/backend/backend.h>
#include <xkbcommon/xkbcommon.h>


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

	uint32_t kmsize;
	int32_t keymap_fd;

	int display_fd;
	uint32_t width, height;
} magma_wl_backend_t;

struct xkb_keymap *magma_wl_backend_get_xkbmap(magma_backend_t *backend, struct xkb_context *context);
struct xkb_state *magma_wl_backend_get_xkbstate(magma_backend_t *backend, struct xkb_keymap *keymap);
void wl_seat_name(void *data, struct wl_seat *seat, const char *name);
void wl_seat_capabilities(void *data, struct wl_seat *seat, uint32_t capabilities);
