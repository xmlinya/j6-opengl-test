
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>

#include <omap_drm.h>
#include <omap_drmif.h>

#include "display-kms.h"
#include <omap_drm.h>
#include <omap_drmif.h>
#include <xf86drmMode.h>

typedef enum {
	false = 0,
	true = 1
} bool;

#ifndef container_of
#define container_of(ptr, type, member) \
    (type *)((char *)(ptr) - (char *) &((type *)0)->member)
#endif

#define to_display_kms(x) container_of(x, struct display_kms, base)

#define DRM_MODE_OBJECT_PLANE 0xeeeeeeee


struct display {
	int fd;
	uint32_t width, height;
	struct omap_device *dev;
	unsigned int bo_flags;
//	int use_cmem;
/*	
	struct buffer ** (*get_buffers)(struct display *disp, uint32_t n);
	struct buffer ** (*get_vid_buffers)(struct display *disp,
			uint32_t n, uint32_t fourcc, uint32_t w, uint32_t h);
	int (*post_buffer)(struct display *disp, struct buffer *buf);
	int (*post_vid_buffer)(struct display *disp, struct buffer *buf,
			uint32_t x, uint32_t y, uint32_t w, uint32_t h);
	void (*close)(struct display *disp);
	void (*disp_free_buf) (struct display *disp, uint32_t n);
*/
	bool multiplanar;	/* True when Y and U/V are in separate buffers. */
//	struct buffer **buf;
};

struct connector {
	uint32_t id;
	char mode_str[64];
	drmModeModeInfo *mode;
	drmModeEncoder *encoder;
	int crtc;
	int pipe;
};

struct display_kms {
       struct display base;
 
       uint32_t connectors_count;
       struct connector connector[10];
       drmModePlane *ovr[10];

       int scheduled_flips, completed_flips;
       uint32_t bo_flags;
       drmModeResPtr resources;
       drmModePlaneRes *plane_resources;
       int mode_setting;
       bool no_master;
       int mastership;
};


static int global_fd = 0;
static uint32_t used_planes = 0;
static int ndisplays = 0;

unsigned int disp_get_width(disp_hander_t han){
	struct display *disp = (struct display *) han;
	return disp->width;
}

unsigned int disp_get_height(disp_hander_t han){
	struct display *disp = (struct display *) han;
	return disp->height;
}

int disp_get_fd(disp_hander_t han){
	struct display *disp = (struct display *) han;
	return disp->fd;
}



static void
page_flip_handler(int fd, unsigned int frame,
		unsigned int sec, unsigned int usec, void *data)
{
	struct display *disp = data;
	struct display_kms *disp_kms = to_display_kms(disp);

	disp_kms->completed_flips++;

	//printf("Page flip: frame=%d, sec=%d, usec=%d, remaining=%d", frame, sec, usec,
	//		disp_kms->scheduled_flips - disp_kms->completed_flips);
}

int disp_post_buffer_noblock(disp_hander_t han,  int fb_id)
{
	struct display *disp = (struct display *) han;
	struct display_kms *disp_kms = to_display_kms(disp);
//	struct buffer_kms *buf_kms = to_buffer_kms(buf);
	int ret, last_err = 0, x = 0;
	uint32_t i;

	for (i = 0; i < disp_kms->connectors_count; i++) {
		struct connector *connector = &disp_kms->connector[i];

		if (! connector->mode) {
			continue;
		}

		if (! disp_kms->mode_setting) {
			/* first buffer we flip to, setup the mode (since this can't
			 * be done earlier without a buffer to scanout)
			 */
			printf("Setting mode %s on connector %d, crtc %d\n",
					connector->mode_str, connector->id, connector->crtc);

			ret = drmModeSetCrtc(disp->fd, connector->crtc, fb_id,
					x, 0, &connector->id, 1, connector->mode);

			x += connector->mode->hdisplay;
		} else {
			ret = drmModePageFlip(disp->fd, connector->crtc, fb_id,
					DRM_MODE_PAGE_FLIP_EVENT, disp);
			disp_kms->scheduled_flips++;
		}

		if (ret) {
			printf("Could not post buffer on crtc %d: %s (%d)",
					connector->crtc, strerror(errno), ret);
			last_err = ret;
			/* well, keep trying the reset of the connectors.. */
		}
	}
	disp_kms->mode_setting = true;
	return last_err;
}

int disp_wait_for_complete(disp_hander_t han)
{
	struct display *disp = (struct display *) han;
	struct display_kms *disp_kms = to_display_kms(disp);
	int ret, last_err = 0;
	while (disp_kms->scheduled_flips > disp_kms->completed_flips) {
		drmEventContext evctx = {
				.version = DRM_EVENT_CONTEXT_VERSION,
				.page_flip_handler = page_flip_handler,
		};
		struct timeval timeout = {
				.tv_sec = 3,
				.tv_usec = 0,
		};
		fd_set fds;

		FD_ZERO(&fds);
		FD_SET(disp->fd, &fds);

		ret = select(disp->fd + 1, &fds, NULL, NULL, &timeout);
		if (ret <= 0) {
			if (errno == EAGAIN) {
				continue;    /* keep going */
			} else {
				printf("Timeout waiting for flip complete: %s (%d)",
						strerror(errno), ret);
				last_err = ret;
				break;
			}
		}

		drmHandleEvent(disp->fd, &evctx);
	}
	return 0;
}

int disp_post_buffer(disp_hander_t han,  int fb_id)
{
	struct display *disp = (struct display *) han;
	struct display_kms *disp_kms = to_display_kms(disp);
//	struct buffer_kms *buf_kms = to_buffer_kms(buf);
	int ret, last_err = 0, x = 0;
	uint32_t i;

	for (i = 0; i < disp_kms->connectors_count; i++) {
		struct connector *connector = &disp_kms->connector[i];

		if (! connector->mode) {
			continue;
		}

		if (! disp_kms->mode_setting) {
			/* first buffer we flip to, setup the mode (since this can't
			 * be done earlier without a buffer to scanout)
			 */
			printf("Setting mode %s on connector %d, crtc %d\n",
					connector->mode_str, connector->id, connector->crtc);

			ret = drmModeSetCrtc(disp->fd, connector->crtc, fb_id,
					x, 0, &connector->id, 1, connector->mode);

			x += connector->mode->hdisplay;
		} else {
			ret = drmModePageFlip(disp->fd, connector->crtc, fb_id,
					DRM_MODE_PAGE_FLIP_EVENT, disp);
			disp_kms->scheduled_flips++;
		}

		if (ret) {
			printf("Could not post buffer on crtc %d: %s (%d)",
					connector->crtc, strerror(errno), ret);
			last_err = ret;
			/* well, keep trying the reset of the connectors.. */
		}
	}

	/* if we flipped, wait for all flips to complete! */
	while (disp_kms->scheduled_flips > disp_kms->completed_flips) {
		drmEventContext evctx = {
				.version = DRM_EVENT_CONTEXT_VERSION,
				.page_flip_handler = page_flip_handler,
		};
		struct timeval timeout = {
				.tv_sec = 3,
				.tv_usec = 0,
		};
		fd_set fds;

		FD_ZERO(&fds);
		FD_SET(disp->fd, &fds);

		ret = select(disp->fd + 1, &fds, NULL, NULL, &timeout);
		if (ret <= 0) {
			if (errno == EAGAIN) {
				continue;    /* keep going */
			} else {
				printf("Timeout waiting for flip complete: %s (%d)",
						strerror(errno), ret);
				last_err = ret;
				break;
			}
		}

		drmHandleEvent(disp->fd, &evctx);
	}

	disp_kms->mode_setting = true;

	return last_err;
}


void
disp_close(disp_hander_t han)
{
	struct display *disp = (struct display *) han;
	omap_device_del(disp->dev);
	disp->dev = NULL;
	if (used_planes) {
		used_planes >>= 1;
	}
	if (--ndisplays == 0) {
		close(global_fd);
	}
	

}

static void
connector_find_mode(struct display * disp, struct connector *c)
{
	struct display_kms *disp_kms = to_display_kms(disp);
	drmModeConnector *connector;
	int i, j;

	/* First, find the connector & mode */
	c->mode = NULL;
	for (i = 0; i < disp_kms->resources->count_connectors; i++) {
		connector = drmModeGetConnector(disp->fd,
				disp_kms->resources->connectors[i]);

		if (!connector) {
			printf("could not get connector %i: %s",
					disp_kms->resources->connectors[i], strerror(errno));
			drmModeFreeConnector(connector);
			continue;
		}

		if (!connector->count_modes) {
			drmModeFreeConnector(connector);
			continue;
		}

		if (connector->connector_id != c->id) {
			drmModeFreeConnector(connector);
			continue;
		}

		for (j = 0; j < connector->count_modes; j++) {
			c->mode = &connector->modes[j];
			if (!strcmp(c->mode->name, c->mode_str))
				break;
		}

		/* Found it, break out */
		if (c->mode)
			break;

		drmModeFreeConnector(connector);
	}

	if (!c->mode) {
		printf("failed to find mode \"%s\"", c->mode_str);
		return;
	}

	/* Now get the encoder */
	for (i = 0; i < disp_kms->resources->count_encoders; i++) {
		c->encoder = drmModeGetEncoder(disp->fd,
				disp_kms->resources->encoders[i]);

		if (!c->encoder) {
			printf("could not get encoder %i: %s",
					disp_kms->resources->encoders[i], strerror(errno));
			drmModeFreeEncoder(c->encoder);
			continue;
		}

		if (c->encoder->encoder_id  == connector->encoder_id)
			break;

		drmModeFreeEncoder(c->encoder);
	}

	if (c->crtc == -1)
		c->crtc = c->encoder->crtc_id;

	/* and figure out which crtc index it is: */
	for (i = 0; i < disp_kms->resources->count_crtcs; i++) {
		if (c->crtc == (int)disp_kms->resources->crtcs[i]) {
			c->pipe = i;
			break;
		}
	}
}


disp_hander_t
disp_kms_open(int disp_id, char* resolution)
{
	struct display_kms *disp_kms = NULL;
	struct display *disp;
	int i;
	
		
	disp_kms = calloc(1, sizeof(*disp_kms));
	if (!disp_kms) {
		printf("allocation failed");
		goto fail;
	}
	disp = &disp_kms->base;

	if (!global_fd) {
		global_fd = drmOpen("omapdrm", NULL);
		if (global_fd < 0) {
			printf("could not open drm device: %s (%d)", strerror(errno), errno);
			goto fail;
		}
	}

	disp->fd = global_fd;
	//disp->use_cmem = use_cmem;
	ndisplays++;  /* increment the number of displays counter */

	disp->dev = omap_device_new(disp->fd);
	if (!disp->dev) {
		printf("couldn't create device");
		goto fail;
	}

	disp_kms->resources = drmModeGetResources(disp->fd);
	if (!disp_kms->resources) {
		printf("drmModeGetResources failed: %s", strerror(errno));
		goto fail;
	}

	disp_kms->plane_resources = drmModeGetPlaneResources(disp->fd);
	if (!disp_kms->plane_resources) {
		printf("drmModeGetPlaneResources failed: %s", strerror(errno));
		goto fail;
	}

	disp->multiplanar = true;

    struct connector *connector =
					&disp_kms->connector[disp_kms->connectors_count++];
	connector->crtc = -1;
    connector->id = disp_id;
    if(resolution)
		strcpy(connector->mode_str, resolution);
    else
		strcpy(connector->mode_str, "1280x720");

	disp->width = 0;
	disp->height = 0;
	for (i = 0; i < (int)disp_kms->connectors_count; i++) {
		struct connector *c = &disp_kms->connector[i];
		connector_find_mode(disp, c);
		if (c->mode == NULL)
			continue;
		/* setup side-by-side virtual display */
		disp->width += c->mode->hdisplay;
		if (disp->height < c->mode->vdisplay) {
			disp->height = c->mode->vdisplay;
		}
	}

	printf("using %d connectors, %dx%d display, multiplanar: %d\n",
			disp_kms->connectors_count, disp->width, disp->height, disp->multiplanar);

	return disp;

fail:
	// XXX cleanup
	return NULL;
}
