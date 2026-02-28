// SPDX-License-Identifier: LGPL-3.0-or-later
// DRM backend implementation

#include "drm.h"

int drm_fd = -1;
int bufferId = -1;
int crtcId = -1;
int pixelFormat = 0;
void *drm_mmap = MAP_FAILED;

void drm_findActiveCrtc(void) {
	drmModeConnector *conn = NULL;
	drmModeEncoder *enc = NULL;
	drmModeRes *res = NULL;
	int i;

	res = drmModeGetResources(drm_fd);
	if (!res) {
		LOG(" drmModeGetResources failed.\n");
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
		LOG(" No active DRM connector found.\n");
		drmModeFreeResources(res);
		exit(EXIT_FAILURE);
	}

	enc = drmModeGetEncoder(drm_fd, conn->encoder_id);
	if (!enc) {
		LOG(" drmModeGetEncoder failed (encoder_id=%u).\n", conn->encoder_id);
		drmModeFreeConnector(conn);
		drmModeFreeResources(res);
		exit(EXIT_FAILURE);
	}

	crtcId = enc->crtc_id;

	drmModeFreeEncoder(enc);
	drmModeFreeConnector(conn);
	drmModeFreeResources(res);
}

int drm_initFrameBuffer(void) {
	int prime_fd;

	LOG("-- Initializing DRM framebuffer device --\n");

	drm_fd = open(DRM_DEVICE, O_RDONLY);
	if (drm_fd == -1) {
		LOG(" Cannot open DRM framebuffer device.\n");
		return -1; // Return to the selector
	} else {
		LOG(" The DRM framebuffer device has been attached.\n");
	}

	if (drmDropMaster(drm_fd) != 0) {
		if (errno != EPERM && errno != EINVAL) {
			LOG(" drmDropMaster failed: %s\n", strerror(errno));
			close(drm_fd);
			exit(EXIT_FAILURE);
		} else {
			LOG(" DRM master not owned, drop not required.\n");
		}
	} else {
		LOG(" DRM master dropped successfully.\n");
	}

	drm_findActiveCrtc();
	drmModeCrtc *crtc = drmModeGetCrtc(drm_fd, crtcId);
	if (!crtc) {
		LOG(" drmModeGetCrtc failed (crtc_id=%u).\n", crtcId);
		exit(EXIT_FAILURE);
	}

	drmModeFB2 *buffer = drmModeGetFB2(drm_fd, crtc->buffer_id);
	if (!buffer) {
		LOG(" drmModeGetFB2 failed (buffer_id=%u).\n", crtc->buffer_id);
		drmModeFreeCrtc(crtc);
		exit(EXIT_FAILURE);
	}

	bufferId = buffer->fb_id;

	if (drmPrimeHandleToFD(drm_fd, buffer->handles[0], DRM_CLOEXEC | DRM_RDWR, &prime_fd) != 0) {
		LOG(" drmPrimeHandleToFD failed (handle=%u).\n", buffer->handles[0]);
		drmModeFreeFB2(buffer);
		drmModeFreeCrtc(crtc);
		exit(EXIT_FAILURE);
	}

	screenInfo.width = buffer->width;
	screenInfo.height = buffer->height;
	screenInfo.stride = buffer->pitches[0];

	pixelFormat = buffer->pixel_format;

	// DRM debug information
	LOG(" Screen width: %d px, height: %d px.\n", screenInfo.width, screenInfo.height);
	LOG(" Stride: %d bytes, FourCC format: %.4s.\n", screenInfo.stride, (char *)&pixelFormat);

	drm_updateScreenFormat();

	drm_mmap = mmap(NULL, screenInfo.stride * screenInfo.height, PROT_READ, MAP_SHARED, prime_fd, 0);
	close(prime_fd);

	if (drm_mmap == MAP_FAILED) {
		LOG(" mmap of DRM framebuffer failed.\n");
		drmModeFreeFB2(buffer);
		drmModeFreeCrtc(crtc);
		exit(EXIT_FAILURE);
	}

	drmModeFreeFB2(buffer);
	drmModeFreeCrtc(crtc);

	return 0;
}

void drm_closeFrameBuffer(void) {
	if (drm_mmap != MAP_FAILED)
		munmap(drm_mmap, screenInfo.stride * screenInfo.height);

	if (drm_fd != -1)
		close(drm_fd);

	// Reset all framebuffer values
	drm_mmap = MAP_FAILED;
	drm_fd = -1;
	bufferId = -1;
	crtcId = -1;

	LOG(" DRM device detached.\n");
}

void drm_updateFrameBufferInfo(void) {
	drmModeCrtc *crtc = drmModeGetCrtc(drm_fd, crtcId);
	if (!crtc) {
		LOG(" drmModeGetCrtc failed (crtc_id=%u). DRM state lost.\n", crtcId);
		exit(EXIT_FAILURE);
	}

	if (crtc->buffer_id != bufferId) {
		drmModeFB2 *buffer = drmModeGetFB2(drm_fd, crtc->buffer_id);
		if (!buffer) {
			LOG(" drmModeGetFB2 failed (buffer_id=%u). DRM state lost.\n", crtc->buffer_id);
			drmModeFreeCrtc(crtc);
			exit(EXIT_FAILURE);
		}

		bufferId = buffer->fb_id;

		screenInfo.width = buffer->width;
		screenInfo.height = buffer->height;
		screenInfo.stride = buffer->pitches[0];

		pixelFormat = buffer->pixel_format;

		drm_updateScreenFormat();

		drmModeFreeFB2(buffer);
	}

	drmModeFreeCrtc(crtc);
}

int drm_checkResolutionChange(void) {
	if ((screenInfo.width != screenFormat.width) || (screenInfo.height != screenFormat.height)) {
		LOG("-- Screen resolution changed from %dx%d to %dx%d --\n",
			screenFormat.width, screenFormat.height,
			screenInfo.width, screenInfo.height);
		drm_updateScreenFormat();
		return 1;
	} else {
		return 0;
	}
}

void drm_updateScreenFormat(void) {
	screenFormat.width = screenInfo.width;
	screenFormat.height = screenInfo.height;
	screenFormat.size = screenInfo.width * screenInfo.height * 4;

	switch (pixelFormat) {

		case DRM_FORMAT_XRGB8888:
		case DRM_FORMAT_ARGB8888:
			screenFormat.bitsPerPixel	= 32;
			screenFormat.redShift		= 16;
			screenFormat.greenShift		= 8;
			screenFormat.blueShift		= 0;
			screenFormat.redMax		= 8;
			screenFormat.greenMax		= 8;
			screenFormat.blueMax		= 8;
			break;

		case DRM_FORMAT_ABGR8888:
			screenFormat.bitsPerPixel	= 32;
			screenFormat.redShift		= 0;
			screenFormat.greenShift		= 8;
			screenFormat.blueShift		= 16;
			screenFormat.redMax		= 8;
			screenFormat.greenMax		= 8;
			screenFormat.blueMax		= 8;
			break;

		default:
			LOG(" Unsupported pixel format: 0x%x. Exiting.\n", pixelFormat);
			exit(EXIT_FAILURE);
	}
}

uint32_t *drm_readFrameBuffer(void) {
	drm_updateFrameBufferInfo();
	return (uint32_t *)drm_mmap;
}
