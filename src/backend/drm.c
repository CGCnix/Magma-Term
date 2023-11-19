#include "magma/backend/backend.h"
#include <poll.h>
#include <stdbool.h>
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
#include <magma/logger/log.h>

#include <sys/ioctl.h>
#include <sys/vt.h>

#define UNUSED(x) ((void)x)

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
} magma_drm_backend_t;

drmModeConnectorPtr magma_drm_backend_find_first_connector(int fd, uint32_t *connectors, int connector_count) {
	int i;
	for(i = 0; i < connector_count; i++) {
		drmModeConnectorPtr connector = drmModeGetConnector(fd, connectors[i]);
		if(!connector) {
			magma_log_error("Failed to get connector id(%d) %m\n", connectors[i]);
			continue;
		}

		if(connector->connection == DRM_MODE_CONNECTED) {
			return connector;
		}

		drmModeFreeConnector(connector);
	}

	return NULL;
}

magma_drm_fb_t *magma_drm_backend_create_fb(int fd, uint32_t width, uint32_t height, uint32_t bpp, uint32_t depth) {
	magma_drm_fb_t *fb;
	int res; 

	fb = calloc(1, sizeof(magma_drm_fb_t));
	if(!fb) {
		magma_log_error("Failed to allocate fb structure %m\n");
		goto err_calloc;
	}

	fb->width = width;
	fb->height = height;
	fb->bpp = bpp;
	fb->depth = depth;

	
	res = drmModeCreateDumbBuffer(fd, width, height, bpp, 0, &fb->handle, &fb->pitch, &fb->size);
	if(res < 0) {
		magma_log_error("Failed to create drm dumb buffer %m\n");
		goto err_create_dumb;
	}

	res = drmModeMapDumbBuffer(fd, fb->handle, &fb->offset);
	if(res < 0) {
		magma_log_error("Failed to map dumb buffer %m\n");
		goto err_map_dumb;
	}

	res = drmModeAddFB(fd, width, height, depth, bpp, fb->pitch, fb->handle, &fb->fb_id);
	if(res < 0) {
		magma_log_error("Failed to add drm fb %m\n");
		goto err_map_dumb;
	}

	fb->data = mmap(NULL, fb->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, fb->offset);
	if(fb->data == MAP_FAILED) {
		magma_log_error("Failed to mmap DRM buffer\n");
		goto err_mmap;
	}

	return fb;


err_mmap:
	drmModeRmFB(fd, fb->fb_id);
err_map_dumb:
	drmModeDestroyDumbBuffer(fd, fb->handle);
err_create_dumb:
	free(fb);
err_calloc:
	return NULL;
}

void magma_drm_backend_start(magma_backend_t *backend) {
	magma_drm_backend_t *drm = (void *)backend;
	if(drm->impl.keymap) {
		drm->impl.keymap(backend, drm->impl.keymap_data);
	}

	if(drmModeSetCrtc(drm->fd, drm->crtc->crtc_id, drm->fb->fb_id, 0, 0, &drm->connector->connector_id, 1, &drm->connector->modes[0])) {
		magma_log_fatal("Failed to set CRTC\n %d %d %d\n %d %d %d %d\n%m\n", drm->fd, drm->crtc->crtc_id, drm->fb->fb_id, 0, 0, drm->connector->connector_id, 1);
		exit(1);
	}
}

void magma_drm_backend_deinit(magma_backend_t *backend) {
	magma_drm_backend_t *drm = (void *)backend;

	drmModeSetCrtc(drm->fd, drm->crtc->crtc_id, drm->crtc->buffer_id, 0, 0, &drm->connector->connector_id, 1, &drm->crtc->mode);
}

/* Less a thing for here but backends in general I want to move
 * the xkb code out of the backends and move them into main that
 * way we only have one set of code to init it and it would allow 
 * use to implement Kitty and Xterms extended keycodes if we wanted 
 * to as the UTF for certain keys is the same so we would need the 
 * actual key code in the terminal code to implement those
 * -Cat
 */

void magma_drm_xkb_update_mods(struct xkb_state *state, int pressed, xkb_mod_mask_t new_depressed) {
	xkb_mod_mask_t depressed = xkb_state_serialize_mods(state, XKB_STATE_DEPRESSED);

	if(pressed) {
		depressed |= new_depressed;
	} else {
		depressed &= ~new_depressed;
	}

	xkb_state_update_mask(state, depressed, 
			0, 0, 0, 0, 0);
}

struct xkb_keymap *magma_drm_backend_get_xkbmap(magma_backend_t *backend, struct xkb_context *context) {
	/*TODO: make is so people can get different keymaps*/
	UNUSED(backend);
	struct xkb_rule_names names = {
		.rules = NULL,
		.model = "pc105",
		.layout = "de",
		.variant = NULL,
		.options = "terminate:ctrl_alt_bksp"

	};

	return xkb_keymap_new_from_names(context, &names, XKB_KEYMAP_COMPILE_NO_FLAGS);
}

struct xkb_state *magma_drm_backend_get_xkbstate(magma_backend_t *backend, struct xkb_keymap *keymap) {
	return xkb_state_new(keymap);
	UNUSED(backend);
}

void magma_drm_backend_key_press(magma_drm_backend_t *drm, uint32_t detail, int32_t value) {
	if(drm->impl.key_press) {	
		drm->impl.key_press((void*)drm, detail, value, drm->impl.key_data);
	}
}

void magma_drm_backend_key(magma_drm_backend_t *drm) {
	struct input_event ev;

	read(drm->keyfd, &ev, 24);

	if(ev.type == EV_KEY) {
		magma_drm_backend_key_press(drm, ev.code + 8, ev.value);
	}
}

void magma_drm_backend_dispatch(magma_backend_t *backend) {
	magma_drm_backend_t *drm = (void *)backend;
	struct pollfd pfd;
	drm->impl.resize(backend, drm->fb->height, drm->fb->width, drm->impl.resize_data);
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

/* Linux will prevent us from playing with the 
 * graphics framebuffer on CRTCs if we don't 
 * have the master LOCK for this FD so lets check
 * that we actually have the lock
 */
bool magma_drm_check_master(int fd) {
	/* Check we aren't already the 
	 * master as many drivers do
	 * this when you are the first 
	 * program to open them 
	 */
	if(drmIsMaster(fd) == 0) {
		return true;
	}

	/* If we are not it might be
	 * Some other program is the 
	 * master or it might just 
	 * be that driver didn't 
	 * auto set us as the master
	 */

	if(drmSetMaster(fd) == 0) {
		return true;
	};

	return false;
}

magma_backend_t *magma_drm_backend_init(void) {
	magma_drm_backend_t *drm;
	char *drm_dev_path, *key_dev_path;
	

	drm = calloc(1, sizeof(magma_drm_backend_t));
	if(!drm) {
		magma_log_error("drm: Failed to allocate drm backend structure %m\n");
		goto err_drm_alloc;
	}

	drm_dev_path = getenv("MAGMA_DRM_DEV") ? getenv("MAGMA_DRM_DEV") : "/dev/dri/card0";
	
	/* maybe try to find keyboard dynamically as event0 is just my keyboard
	 * path this won't be the same on any two systems and could leave a user
	 * in hard to escape situation 
	 */
	key_dev_path = getenv("MAGMA_KEY_DEV") ? getenv("MAGMA_KEY_DEV") : "/dev/input/event0";

	drm->fd = open(drm_dev_path, O_RDWR | O_CLOEXEC);
	if (drm->fd < 0) {
		magma_log_error("drm: Failed to open %s %m\n", drm_dev_path);
		goto err_open_drm;
	}

	magma_log_warn("%d %d\n", drmIsKMS(drm->fd), drmIsMaster(drm->fd));
	if(!magma_drm_check_master(drm->fd)) {
		goto err_not_master;
	}

	drm->keyfd = open(key_dev_path, O_RDWR | O_CLOEXEC);
	if (drm->keyfd < 0) {
		magma_log_error("drm: Failed to open %s %m\n", drm_dev_path);
		goto err_open_keyboard;
	}

	drm->res = drmModeGetResources(drm->fd);
	if (!drm->res) {
		magma_log_error("Failed to get drm resources %m\n");
		goto err_get_res;
	}

	drm->connector = magma_drm_backend_find_first_connector(drm->fd, drm->res->connectors, drm->res->count_connectors);
	if(!drm->connector || !drm->connector->encoder_id) {
		magma_log_error("Failed to get connector with valid encoder %p %m\n", drm->connector);
		goto err_get_connector;
	}

	drm->encoder = drmModeGetEncoder(drm->fd, drm->connector->encoder_id);
	if(!drm->encoder || !drm->encoder->crtc_id) {
		magma_log_error("Failed to get encoder with valid crtc %p %m\n", drm->encoder);
		goto err_get_encoder;
	}

	drm->crtc = drmModeGetCrtc(drm->fd, drm->encoder->crtc_id);
	if(!drm->crtc) {
		magma_log_error("Failed to get CRTC: %p %m\n");
		goto err_get_crtc;
	}

	drm->fb = magma_drm_backend_create_fb(drm->fd, 0, 0, 32, 24);
	if(!drm->fb) {
		magma_log_error("Failed to allocate FB: %p %m\n");
		goto err_create_fb;
	}

	magma_log_fatal("This DRM backend is a mess we refuse to start it without you taking some knowledge that you are starting messy code");
	exit(1);

	drm->impl.start = magma_drm_backend_start;
	drm->impl.dispatch_events = magma_drm_backend_dispatch;
	drm->impl.put_buffer = magma_drm_backend_put_buffer;
	drm->impl.deinit = magma_drm_backend_deinit;
	drm->impl.get_kmap = magma_drm_backend_get_xkbmap;
	drm->impl.get_state = magma_drm_backend_get_xkbstate;
	return (void *)drm;

err_create_fb:
	drmModeFreeCrtc(drm->crtc);
err_get_crtc:
	drmModeFreeEncoder(drm->encoder);
err_get_encoder:
	drmModeFreeConnector(drm->connector);
err_get_connector:
	drmModeFreeResources(drm->res);
err_get_res:
	close(drm->keyfd);
err_open_keyboard:
err_not_master:
	close(drm->fd);
err_open_drm:
	free(drm);
err_drm_alloc:
	return NULL;
}

