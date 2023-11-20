#pragma once

#include <stdint.h>
#include <magma/backend/backend.h>
#include <xkbcommon/xkbcommon.h>

#include <vulkan/vulkan.h>

struct magma_backend {
	void (*start)(magma_backend_t *backend);
	void (*deinit)(magma_backend_t *backend);
	void (*dispatch_events)(magma_backend_t *backend);

	PFN_MAGMADRAWCB draw;
	PFN_MAGMARESIZCB resize;
	PFN_MAGMACLOSECB close;
	PFN_MAGMAKEYCB key_press;
	PFN_MAGMAKEYMAPCB keymap;

	/*TODO: IMPLEMENT*/
	void (*button_press)(magma_backend_t *backend);
	void (*enter)(magma_backend_t *backend);
	void (*cursor_motion)(magma_backend_t *backends);
	
	/*call backend*/
	void (*put_buffer)(magma_backend_t *backend, magma_buf_t *buffer);
	struct xkb_keymap *(*get_kmap)(magma_backend_t *backend, struct xkb_context *context);
	struct xkb_state *(*get_state)(magma_backend_t *backend, struct xkb_keymap *keymap);
	void (*magma_backend_get_vk_exts)(magma_backend_t *backend, char ***ext_names, uint32_t *size);
	VkResult (*magma_backend_get_vk_surface)(magma_backend_t *backend, VkInstance instance, VkSurfaceKHR *surface);

	void *keymap_data;
	void *draw_data;
	void *resize_data;
	void *button_data;
	void *key_data;
	void *enter_data;
	void *cursor_data;
	void *close_data;
};
