/*Wayland Libraries*/
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>

/*Std C*/
#include <stdlib.h>

/*Magma Headers*/
#include <magma/private/backend/backend.h>
#include <magma/backend/backend.h>

typedef struct magma_wl_backend {
	struct wl_display *display;
	struct wl_registry *registry;

} magma_wl_backend_t;

magma_backend_t *magma_wl_backend_init() {
	magma_wl_backend_t *wl;

	wl = calloc(1, sizeof(magma_wl_backend_t));

	return NULL;
}
