#pragma once

#include <stdint.h>

#include <xkbcommon/xkbcommon.h>

typedef struct magma_backend magma_backend_t;

/** 
 * Buffers Auto assume RGB FORMAT
 * and should be used a fallback when no
 * rendering through hardware is avliable
 */
typedef struct magma_buf {
	uint32_t width, height;
	uint8_t depth, bpp;
	void *buffer;
}magma_buf_t;

#define MAGMA_KEY_PRESS 1
#define MAGMA_KEY_RELEASE 0

typedef void (*PFN_MAGMADRAWCB)(magma_backend_t *backend, uint32_t height, uint32_t width, void *data);
typedef void (*PFN_MAGMACLOSECB)(magma_backend_t *backend, void *data);
typedef void (*PFN_MAGMARESIZCB)(magma_backend_t *backend, uint32_t height, uint32_t width, void *data);
typedef void (*PFN_MAGMAKEYCB)(magma_backend_t *backend, int key, int action, void *data);
typedef void (*PFN_MAGMAKEYMAPCB)(magma_backend_t *backend, void *data);
/**
 *	@brief generate a backend from name
 *	@param [in] string to select backend from
 *	@retval NULL returned on error backend name doesn't exist
 *	@retval !NULL a pointer to the backend structure
 *	@return a pointer to the backend
 *	
 */
magma_backend_t *magma_backend_init_name(const char *name);

/**
 *	@brief generate a backend from environment variables
 *	@retval NULL returned on error backend name doesn't exist
 *	@retval !NULL a pointer to the backend structure
 *	@return a pointer to the backend
 */
magma_backend_t *magma_backend_init_auto(void);


void magma_backend_start(magma_backend_t *backend);
void magma_backend_deinit(magma_backend_t *backend);
void magma_backend_dispatch_events(magma_backend_t *backend);
void magma_backend_set_on_keymap(magma_backend_t *backend, PFN_MAGMAKEYMAPCB keymap, void *data);
void magma_backend_set_on_draw(magma_backend_t *backend, PFN_MAGMADRAWCB draw, void *data);
void magma_backend_set_on_resize(magma_backend_t *backend, PFN_MAGMARESIZCB resize, void *data);
void magma_backend_set_on_close(magma_backend_t *backend, PFN_MAGMACLOSECB closefn, void *data);
void magma_backend_set_on_button(magma_backend_t *backend, void (*button_press)(magma_backend_t *backend), void *data);
void magma_backend_set_on_key(magma_backend_t *backend, PFN_MAGMAKEYCB, void *data);
void magma_backend_set_on_enter(magma_backend_t *backend, void (*enter)(magma_backend_t *backend), void *data);
void magma_backend_set_on_cursor(magma_backend_t *backend, void (*cursor_motion)(magma_backend_t *backends), void *data);


struct xkb_keymap *magma_backend_get_xkbmap(magma_backend_t *backend, struct xkb_context *context);
struct xkb_state *magma_backend_get_xkbstate(magma_backend_t *backend, struct xkb_keymap *keymap);
void magma_backend_put_buffer(magma_backend_t *backend, magma_buf_t *buffer);
