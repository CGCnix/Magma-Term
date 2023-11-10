#include "magma/logger/log.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <xcb/xcb.h>
#include <magma/backend/backend.h>
#include <magma/private/backend/backend.h>

#include <xcb/xproto.h>
#include <xcb/xcb_image.h>


#include <xkbcommon/xkbcommon-names.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-x11.h>
#include <xkbcommon/xkbcommon-keysyms.h>


typedef struct magma_xcb_backend {
	magma_backend_t impl;

	xcb_connection_t *connection;
	xcb_window_t window;
	xcb_screen_t *screen;
	xcb_gcontext_t gc;
	xcb_visualid_t visual;
	xcb_colormap_t colormap;
	uint8_t depth;

	struct xkb_context *xkbctx;
	struct xkb_keymap *xkbmap;
	struct xkb_state *xkbstate;
} magma_xcb_backend_t;

void magma_xcb_backend_deinit(magma_backend_t *backend) {
	magma_xcb_backend_t *xcb = (void *)backend;

	xkb_state_unref(xcb->xkbstate);

	xkb_keymap_unref(xcb->xkbmap);

	xkb_context_unref(xcb->xkbctx);

	xcb_destroy_window(xcb->connection, xcb->window);

	xcb_disconnect(xcb->connection);

	free(xcb);
}

void magma_xcb_backend_start(magma_backend_t *backend) {
	magma_xcb_backend_t *xcb = (void *)backend;

	xcb_map_window(xcb->connection, xcb->window);

	xcb_flush(xcb->connection);
}

void magma_xcb_backend_put_buffer(magma_backend_t *backend, magma_buf_t *buffer) {
	magma_xcb_backend_t *xcb = (void *)backend;

	xcb_image_t *image = xcb_image_create(buffer->width, buffer->height, XCB_IMAGE_FORMAT_Z_PIXMAP, buffer->bpp, xcb->depth, buffer->bpp, buffer->bpp, 0, XCB_IMAGE_ORDER_LSB_FIRST, buffer->buffer, buffer->width * buffer->height * 4, buffer->buffer);

	xcb_image_put(xcb->connection, xcb->window, xcb->gc, image, 0, 0, 0);

	xcb_image_destroy(image);
}

/*TODO: IMPLEMENT CALLBACKS*/
void magma_xcb_backend_expose(magma_xcb_backend_t *xcb, xcb_expose_event_t *expose) {
	__builtin_dump_struct(expose, &printf);

	if(xcb->impl.draw) {
		xcb->impl.draw((void *)xcb, expose->height, expose->width, xcb->impl.draw_data);
	}
	

	xcb_flush(xcb->connection);
}

void magma_xcb_backend_configure(magma_xcb_backend_t *xcb, xcb_configure_notify_event_t *notify) {
	if(xcb->impl.resize) {
		xcb->impl.resize((void*)xcb, notify->height, notify->width, xcb->impl.resize_data);
	}
}

void magma_xcb_xkb_update_mods(struct xkb_state *state, int pressed, xkb_mod_mask_t new_depressed) {
	xkb_mod_mask_t depressed = xkb_state_serialize_mods(state, XKB_STATE_DEPRESSED);

	if(pressed) {
		depressed |= new_depressed;
	} else {
		depressed &= ~new_depressed;
	}

	xkb_state_update_mask(state, depressed, 
			0, 0, 0, 0, 0);
}

void magma_xcb_backend_key_press(magma_xcb_backend_t *xcb, xcb_key_press_event_t *press) {
	char *buffer;
	int length;
	magma_log_info("Got Keypress %d\n", press->detail);
	if(press->detail == 50) {
		magma_xcb_xkb_update_mods(xcb->xkbstate, 1, XCB_MOD_MASK_SHIFT);
		return;
	}
	if(press->detail == 108) {
		magma_xcb_xkb_update_mods(xcb->xkbstate, 1, XCB_MOD_MASK_5);
		return;
	}

	length = xkb_state_key_get_utf8(xcb->xkbstate, press->detail, NULL, 0) + 1;
	
	buffer = calloc(1, length);

	xkb_state_key_get_utf8(xcb->xkbstate, press->detail, buffer, length);
	printf("Calling Key callback\n");
	if(xcb->impl.key_press) {	
		xcb->impl.key_press((void*)xcb, buffer, length - 1, xcb->impl.key_data);
	}

}

void magma_xcb_backend_key_release(magma_xcb_backend_t *xcb, xcb_key_release_event_t *release) {
	if(release->detail == 50) {
		magma_xcb_xkb_update_mods(xcb->xkbstate, 0, XCB_MOD_MASK_SHIFT);
	}
	if(release->detail == 108) {
		magma_xcb_xkb_update_mods(xcb->xkbstate, 0, XCB_MOD_MASK_5);
		return;
	}


}


void magma_xcb_backend_button_press(magma_xcb_backend_t *xcb, xcb_button_press_event_t *press) {

}

void magma_xcb_backend_button_release(magma_xcb_backend_t *xcb, xcb_button_release_event_t *release) {

}

void magma_xcb_backend_pointer_motion(magma_xcb_backend_t *xcb, xcb_motion_notify_event_t *motion) {

}

void magma_xcb_backend_enter(magma_xcb_backend_t *xcb, xcb_enter_notify_event_t *enter) {

}

void magma_xcb_backend_leave(magma_xcb_backend_t *xcb, xcb_leave_notify_event_t *leave) {

}

void magma_xcb_backend_dispacth(magma_backend_t *backend) {
	xcb_generic_event_t *event;
	magma_xcb_backend_t *xcb = (void *)backend;
	

	while((event = xcb_poll_for_event(xcb->connection))) {
		switch(event->response_type & ~0x80) {
			case XCB_EXPOSE:
				magma_xcb_backend_expose(xcb, (void *)event);
				break;
			case XCB_CONFIGURE_NOTIFY:
				magma_xcb_backend_configure(xcb, (void*)event);
				break;
			case XCB_KEY_PRESS:
				magma_xcb_backend_key_press(xcb, (void*)event);
				break;
			case XCB_KEY_RELEASE:
				magma_xcb_backend_key_release(xcb, (void*)event);
				break;
			case XCB_ENTER_NOTIFY:
				magma_xcb_backend_enter(xcb, (void*)event);
				break;
			case XCB_LEAVE_NOTIFY:
				magma_xcb_backend_leave(xcb, (void*)event);
				break;
			case XCB_MOTION_NOTIFY:
				magma_xcb_backend_pointer_motion(xcb, (void *)event);
				break;
			case XCB_BUTTON_PRESS:
				magma_xcb_backend_button_press(xcb, (void *)event);
				break;
			case XCB_BUTTON_RELEASE:
				magma_xcb_backend_button_release(xcb, (void *)event);
				break;
			case XCB_MAP_NOTIFY:
				break;
			default:
				printf("magma-xcb: unknown event %d\n", event->response_type);
		}
		xcb_flush(xcb->connection);
		free(event);
	}
}

xcb_visualtype_t *magma_xcb_match_visual(uint32_t depth, const xcb_screen_t *screen) {
	xcb_depth_iterator_t depths_iter;
	xcb_visualtype_iterator_t visual_iter;
	depths_iter = xcb_screen_allowed_depths_iterator(screen);
	
	for(; depths_iter.rem; xcb_depth_next(&depths_iter)) {
		
		if(depths_iter.data->depth == depth) {
			visual_iter = xcb_depth_visuals_iterator(depths_iter.data);
			for(; visual_iter.rem; xcb_visualtype_next(&visual_iter)) {
				magma_log_info("%d\n", visual_iter.data->visual_id);
				if(XCB_VISUAL_CLASS_TRUE_COLOR == visual_iter.data->_class) {
					return visual_iter.data;
				}
			}
		}
	}

	
	return NULL;
}

magma_backend_t *magma_xcb_backend_init() {
	magma_xcb_backend_t *xcb;
	xcb_screen_iterator_t iter;
	const xcb_setup_t *setup;
	xcb_void_cookie_t cookie;
	xcb_visualtype_t *visual;
	xcb_generic_error_t *error;
	int screen_nbr = 0;
	uint32_t mask, values[3];

	xcb = calloc(1, sizeof(*xcb));

	xcb->connection = xcb_connect(NULL, &screen_nbr);

	setup = xcb_get_setup(xcb->connection);

	iter = xcb_setup_roots_iterator(setup);

	for (; iter.rem; --screen_nbr, xcb_screen_next (&iter)) {
		if (screen_nbr == 0) {
			xcb->screen = iter.data;
			break;
		}
	}
	
	visual = magma_xcb_match_visual(32, xcb->screen);
	if(visual == NULL) {
		xcb->visual = xcb->screen->root_visual;
		xcb->depth = xcb->screen->root_depth;
		xcb->colormap = xcb->screen->default_colormap;
	} else {
		xcb->visual = visual->visual_id;
		xcb->depth = 32;
		xcb->colormap = xcb_generate_id(xcb->connection);

		cookie = xcb_create_colormap_checked(xcb->connection, XCB_COLORMAP_ALLOC_NONE, xcb->colormap, xcb->screen->root, xcb->visual);
		error = xcb_request_check(xcb->connection, cookie);
		if(error) {
			magma_log_error("XCB failed to create colormap\n");
			return NULL;
		}
	}

	xcb->window = xcb_generate_id(xcb->connection);
	mask = XCB_CW_BORDER_PIXEL | XCB_CW_EVENT_MASK | XCB_CW_COLORMAP;
	values[0] = 0x000000;
	values[1] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS |
              XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION |
              XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW |
              XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE |
			  XCB_EVENT_MASK_STRUCTURE_NOTIFY;
	values[2] = xcb->colormap;

	cookie = xcb_create_window_checked(xcb->connection, xcb->depth,
			xcb->window, xcb->screen->root, 0, 0, 600, 600, 1, 
			XCB_WINDOW_CLASS_INPUT_OUTPUT, xcb->visual, 
			mask, values);
	
	error = xcb_request_check(xcb->connection, cookie);
	if(error) {
		magma_log_error("Failed to create window: %d.%d.%d\n", error->error_code, error->major_code, error->minor_code);
		return NULL;
	}

	xcb->gc = xcb_generate_id(xcb->connection);
	mask = XCB_GC_FOREGROUND;
	values[0] = xcb->screen->black_pixel;
	cookie = xcb_create_gc_checked(xcb->connection, xcb->gc, xcb->window, mask, values);
	error = xcb_request_check(xcb->connection, cookie);
	if(error) {
		printf("Failed to create GC\n");
		return NULL;
	}

	magma_log_info("XCB Screen Info: %d\n", xcb->screen->root);
	magma_log_info("	width: %d\n", xcb->screen->width_in_pixels);
	magma_log_info("	height: %d\n", xcb->screen->height_in_pixels);

	/*KEYBOARD*/
	int device_id;
	xcb->xkbctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	xkb_x11_setup_xkb_extension(xcb->connection, 1, 0, XKB_X11_SETUP_XKB_EXTENSION_NO_FLAGS,
				NULL, NULL, NULL, NULL);	

	device_id = xkb_x11_get_core_keyboard_device_id(xcb->connection);
	if(device_id < 0) {
		printf("Device id error: %m\n");
		return NULL;	
	}

	xcb->xkbmap = xkb_x11_keymap_new_from_device(xcb->xkbctx, xcb->connection,
			device_id, XKB_KEYMAP_COMPILE_NO_FLAGS);

	xcb->xkbstate = xkb_x11_state_new_from_device(xcb->xkbmap, xcb->connection, device_id);


	xcb->impl.start = magma_xcb_backend_start;
	xcb->impl.deinit = magma_xcb_backend_deinit;
	xcb->impl.dispatch_events = magma_xcb_backend_dispacth;
	xcb->impl.put_buffer = magma_xcb_backend_put_buffer;
	return (void*)xcb;
}
