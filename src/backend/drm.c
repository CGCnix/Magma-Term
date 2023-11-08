#include <poll.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <string.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <unistd.h>
#include <fcntl.h>

#include <linux/input.h>
#include <linux/input-event-codes.h>

#include <sys/mman.h>

#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-x11.h>

#include <magma/private/backend/backend.h>

typedef struct magma_drm_fb {
	uint32_t handle;
	uint32_t fb_id;

	uint32_t height, width;
	uint32_t pitch;
	uint64_t size, offset;
	uint32_t bpp, depth;

	uint32_t *data;
} magma_drm_fb_t;

typedef struct magma_drm_backend {
	magma_backend_t impl;

	int fd;
	
	int keyfd;

	drmModeResPtr res;
	drmModeConnectorPtr connector;
	drmModeEncoderPtr encoder;
	drmModeCrtcPtr crtc;

	magma_drm_fb_t *fb;

	struct xkb_context *xkbctx;
	struct xkb_keymap *xkbmap;
	struct xkb_state *xkbstate;
} magma_drm_backend_t;

	
drmModeConnectorPtr magma_drm_backend_find_first_connector(int fd, uint32_t *connectors, int connector_count) {
	int i;
	for(i = 0; i < connector_count; i++) {
		drmModeConnectorPtr connector = drmModeGetConnector(fd, connectors[i]);

		if(connector->connection == DRM_MODE_CONNECTED) {
			return connector;
		}

		drmModeFreeConnector(connector);
	}

	return NULL;
}

magma_drm_fb_t *magma_drm_backend_create_fb(int fd, uint32_t width, uint32_t height, uint32_t bpp, uint32_t depth) {
	magma_drm_fb_t *fb = calloc(1, sizeof(magma_drm_fb_t));

	fb->width = width;
	fb->height = height;
	fb->bpp = bpp;
	fb->depth = depth;

	drmModeCreateDumbBuffer(fd, width, height, bpp, 0, &fb->handle, &fb->pitch, &fb->size);

	drmModeMapDumbBuffer(fd, fb->handle, &fb->offset);

	drmModeAddFB(fd, width, height, depth, bpp, fb->pitch, fb->handle, &fb->fb_id);

	fb->data = mmap(NULL, fb->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, fb->offset);

	return fb;
}

void magma_drm_backend_start(magma_backend_t *backend) {
	magma_drm_backend_t *drm = (void *)backend;

	drmModeSetCrtc(drm->fd, drm->crtc->crtc_id, drm->fb->fb_id, 0, 0, &drm->connector->connector_id, 1, &drm->connector->modes[0]);
}

void magma_drm_backend_deinit(magma_backend_t *backend) {
	magma_drm_backend_t *drm = (void *)backend;

	drmModeSetCrtc(drm->fd, drm->crtc->crtc_id, drm->crtc->buffer_id, 0, 0, &drm->connector->connector_id, 1, &drm->crtc->mode);
}

void magma_drm_xkb_update_mods(struct xkb_state *state, int pressed, xkb_mod_mask_t new_depressed) {
	xkb_mod_mask_t depressed = xkb_state_serialize_mods(state, XKB_STATE_DEPRESSED);

	printf("MOD MASK: %x, %x\n", depressed, new_depressed);

	if(pressed) {
		depressed |= new_depressed;
	} else {
		depressed &= ~new_depressed;
	}

	xkb_state_update_mask(state, depressed, 
			0, 0, 0, 0, 0);
}

void magma_drm_backend_key_press(magma_drm_backend_t *drm, uint32_t detail) {
	char *buffer;
	int length;
	if(detail == 50) {
		magma_drm_xkb_update_mods(drm->xkbstate, 1, XCB_MOD_MASK_SHIFT);
		return;
	}
	if(detail == 108) {
		magma_drm_xkb_update_mods(drm->xkbstate, 1, XCB_MOD_MASK_5);
		return;
	}

	length = xkb_state_key_get_utf8(drm->xkbstate, detail, NULL, 0) + 1;
	
	buffer = calloc(1, length);

	xkb_state_key_get_utf8(drm->xkbstate, detail, buffer, length);
	

	for(int i = 0; i < length; i++) {
		printf("%u, ", buffer[i] & 0xff);
	}
	printf("\n");

	

	if(drm->impl.key_press) {	
		drm->impl.key_press((void*)drm, buffer, length - 1, drm->impl.key_data);
	}

}

void magma_drm_backend_key_release(magma_drm_backend_t *xcb, uint32_t detail) {
	printf("REL\n");
	if(detail == 50) {
		magma_drm_xkb_update_mods(xcb->xkbstate, 0, XCB_MOD_MASK_SHIFT);
	}
	if(detail == 108) {
		magma_drm_xkb_update_mods(xcb->xkbstate, 0, XCB_MOD_MASK_5);
		return;
	}


}



void magma_drm_backend_key(magma_drm_backend_t *drm) {
	struct input_event ev;

	read(drm->keyfd, &ev, 24);

	if(ev.type == EV_KEY) {
		if(ev.value) {
			magma_drm_backend_key_press(drm, ev.code + 8);
		} else {
			magma_drm_backend_key_release(drm, ev.code + 8);
		}
	}
}

void magma_drm_backend_dispatch(magma_backend_t *backend) {
	magma_drm_backend_t *drm = (void *)backend;
	struct pollfd pfd;

	drm->impl.draw(backend, drm->fb->height, drm->fb->width, drm->impl.draw_data);

	pfd.fd = drm->keyfd;
	pfd.events = POLLIN;
	if(poll(&pfd, 1, 10)) {
		magma_drm_backend_key(drm);	
	}
}

void magma_drm_backend_put_buffer(magma_backend_t *backend, magma_buf_t *buffer) {
	magma_drm_backend_t *drm = (void*)backend;

	memcpy(drm->fb->data, buffer->buffer, buffer->width * buffer->height * 4);
	free(buffer->buffer);
}

magma_backend_t *magma_drm_backend_init() {
	magma_drm_backend_t *drm = calloc(1, sizeof(magma_drm_backend_t));

	drm->fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);

	drm->keyfd = open("/dev/input/event0", O_RDWR | O_CLOEXEC);

	drm->res = drmModeGetResources(drm->fd);

	drm->connector = magma_drm_backend_find_first_connector(drm->fd, drm->res->connectors, drm->res->count_connectors);

	drm->encoder = drmModeGetEncoder(drm->fd, drm->connector->encoder_id);

	drm->crtc = drmModeGetCrtc(drm->fd, drm->encoder->crtc_id);

	drm->fb = magma_drm_backend_create_fb(drm->fd, 1600, 900, 32, 24);

	drm->xkbctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

	struct xkb_rule_names names = {
		.rules = NULL,
		.model = "pc105",
		.layout = "de",
		.variant = NULL,
		.options = "terminate:ctrl_alt_bksp"

	};
	
	drm->xkbmap = xkb_keymap_new_from_names(drm->xkbctx, &names, XKB_KEYMAP_COMPILE_NO_FLAGS);

	drm->xkbstate = xkb_state_new(drm->xkbmap);

	drm->impl.start = magma_drm_backend_start;
	drm->impl.dispatch_events = magma_drm_backend_dispatch;
	drm->impl.put_buffer = magma_drm_backend_put_buffer;
	drm->impl.deinit = magma_drm_backend_deinit;

	return (void *)drm;
}

