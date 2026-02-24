// SPDX-License-Identifier: LGPL-3.0-or-later
// Framebuffer backend implementation

#include "framebuffer.h"

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>

screenformat screenFormat;
drm_screeninfo screenInfo;

int drm_fd = -1;
void *drm_mmap = MAP_FAILED;

int fb_pixels_per_line = 0;

void findActiveCrtc(void) {
	drmModeConnector *conn = NULL;
	drmModeEncoder *enc = NULL;
	drmModeRes *res = NULL;
	int i;

	res = drmModeGetResources(drm_fd);
	if (!res) {
		L(" drmModeGetResources failed.\n");
		exit(EXIT_FAILURE);
	}

	for (i = 0; i < res->count_connectors; i++) {
		conn = drmModeGetConnector(drm_fd, res->connectors[i]);
		if (conn && conn->connection == DRM_MODE_CONNECTED)
			break;

		if (conn)
			drmModeFreeConnector(conn);

		conn = NULL;
	}

	if (!conn) {
		L(" No active DRM connector found.\n");
		drmModeFreeResources(res);
		exit(EXIT_FAILURE);
	}

	enc = drmModeGetEncoder(drm_fd, conn->encoder_id);
	if (!enc) {
		L(" drmModeGetEncoder failed (encoder_id=%u).\n", conn->encoder_id);
		drmModeFreeConnector(conn);
		drmModeFreeResources(res);
		exit(EXIT_FAILURE);
	}

	screenInfo.crtc_id = enc->crtc_id;

	drmModeFreeEncoder(enc);
	drmModeFreeConnector(conn);
	drmModeFreeResources(res);
}

void updateFrameBufferInfo(void) {
	drmModeCrtc *crtc = drmModeGetCrtc(drm_fd, screenInfo.crtc_id);
	if (!crtc) {
		L(" drmModeGetCrtc failed (crtc_id=%u). DRM state lost.\n", screenInfo.crtc_id);
		exit(EXIT_FAILURE);
	}

	if (crtc->buffer_id != screenInfo.current_fb_id) {

		drmModeFB2 *fb = drmModeGetFB2(drm_fd, crtc->buffer_id);
		if (!fb) {
			L(" drmModeGetFB2 failed (buffer_id=%u). DRM state lost.\n", crtc->buffer_id);
			drmModeFreeCrtc(crtc);
			exit(EXIT_FAILURE);
		}

		screenInfo.current_fb_id = fb->fb_id;
		screenInfo.width = fb->width;
		screenInfo.height = fb->height;
		screenInfo.stride = fb->pitches[0];
		screenInfo.pixel_format = fb->pixel_format;

		fb_pixels_per_line = screenInfo.stride / 4;

		updateScreenFormat();

		drmModeFreeFB2(fb);
	}

	drmModeFreeCrtc(crtc);
}

void initFrameBuffer(void) {
	int prime_fd;

	L("-- Initializing DRM device --\n");

	drm_fd = open(DRM_DEVICE, O_RDONLY);
	if (drm_fd == -1) {
		L(" Cannot open DRM device.\n");
		exit(EXIT_FAILURE);
	} else {
		L(" The DRM device has been attached.\n");
	}

	findActiveCrtc();
	drmModeCrtc *crtc = drmModeGetCrtc(drm_fd, screenInfo.crtc_id);
	if (!crtc) {
		L(" drmModeGetCrtc failed (crtc_id=%u).\n", screenInfo.crtc_id);
		exit(EXIT_FAILURE);
	}

	drmModeFB2 *fb = drmModeGetFB2(drm_fd, crtc->buffer_id);
	if (!fb) {
		L(" drmModeGetFB2 failed (buffer_id=%u).\n", crtc->buffer_id);
		drmModeFreeCrtc(crtc);
		exit(EXIT_FAILURE);
	}

	screenInfo.current_fb_id = fb->fb_id;
	screenInfo.width = fb->width;
	screenInfo.height = fb->height;
	screenInfo.stride = fb->pitches[0];
	screenInfo.pixel_format = fb->pixel_format;

	if (drmPrimeHandleToFD(drm_fd, fb->handles[0], DRM_CLOEXEC | DRM_RDWR, &prime_fd) != 0) {
		L(" drmPrimeHandleToFD failed (handle=%u).\n", fb->handles[0]);
		drmModeFreeFB2(fb);
		drmModeFreeCrtc(crtc);
		exit(EXIT_FAILURE);
	}

	drm_mmap = mmap(NULL, screenInfo.stride * screenInfo.height, PROT_READ, MAP_SHARED, prime_fd, 0);
	close(prime_fd);

	if (drm_mmap == MAP_FAILED) {
		L(" mmap of DRM framebuffer failed.\n");
		drmModeFreeFB2(fb);
		drmModeFreeCrtc(crtc);
		exit(EXIT_FAILURE);
	}

	fb_pixels_per_line = screenInfo.stride / 4;

	updateScreenFormat();

	drmModeFreeFB2(fb);
	drmModeFreeCrtc(crtc);
}

int checkResolutionChange(void) {
	if ((screenInfo.width != screenFormat.width) || (screenInfo.height != screenFormat.height)) {
		L("-- Screen resolution changed from %dx%d to %dx%d --\n",
			screenFormat.width, screenFormat.height,
			screenInfo.width, screenInfo.height);
		updateScreenFormat();
		return 1;
	} else {
		return 0;
	}
}

void updateScreenFormat(void) {
	screenFormat.width  = screenInfo.width;
	screenFormat.height = screenInfo.height;
	screenFormat.size   = screenInfo.width * screenInfo.height * 4;

	switch (screenInfo.pixel_format) {

	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_ARGB8888:
		screenFormat.bitsPerPixel = 32;
		screenFormat.redShift   = 16;
		screenFormat.greenShift = 8;
		screenFormat.blueShift  = 0;
		screenFormat.redMax   = 8;
		screenFormat.greenMax = 8;
		screenFormat.blueMax  = 8;
		break;

	default:
		L(" Unsupported pixel format: 0x%x. Exiting.\n", screenInfo.pixel_format);
		exit(EXIT_FAILURE);
	}
}

uint32_t *readFrameBuffer(void) {
	updateFrameBufferInfo();
	return (uint32_t *)drm_mmap;
}

void closeFrameBuffer(void) {
	if (drm_mmap != MAP_FAILED)
		munmap(drm_mmap, screenInfo.stride * screenInfo.height);

	if (drm_fd != -1)
		close(drm_fd);

	drm_mmap = MAP_FAILED;
	drm_fd = -1;

	L(" DRM device detached.\n");
}
