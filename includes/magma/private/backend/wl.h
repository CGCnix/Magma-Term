#pragma once

#include <wayland-client.h>

#include <magma/private/backend/backend.h>


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

	int display_fd;
	uint32_t width, height;
} magma_wl_backend_t;


void wl_seat_name(void *data, struct wl_seat *seat, const char *name);
void wl_seat_capabilities(void *data, struct wl_seat *seat, uint32_t capabilities);
