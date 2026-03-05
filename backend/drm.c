// SPDX-License-Identifier: LGPL-3.0-or-later
// DRM backend implementation

#include "drm.h"

int drmFd = -1;
int bufferId = -1;
int crtcId = -1;
int pixelFormat = 0;
void *drmBufferMap = MAP_FAILED;

void drm_findActiveCrtc(void) {
	drmModeConnector *conn = NULL;
	drmModeEncoder *enc = NULL;
	drmModeRes *res = NULL;
	int i;

	res = drmModeGetResources(drmFd);
	if (!res) {
		LOG(" Failed to query DRM resources.\n");
		exit(EXIT_FAILURE);
	}

	for (i = 0; i < res->count_connectors; i++) {
		conn = drmModeGetConnector(drmFd, res->connectors[i]);
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
	} else {
		LOG(" Active DRM connector: %u.\n", conn->connector_id);
	}

	enc = drmModeGetEncoder(drmFd, conn->encoder_id);
	if (!enc) {
		LOG(" Failed to query encoder: %u.\n", conn->encoder_id);
		drmModeFreeConnector(conn);
		drmModeFreeResources(res);
		exit(EXIT_FAILURE);
	} else {
		LOG(" Encoder in use: %u.\n", conn->encoder_id);
	}

	crtcId = enc->crtc_id;

	drmModeFreeEncoder(enc);
	drmModeFreeConnector(conn);
	drmModeFreeResources(res);
}

int drm_initFrameBuffer(void) {
	int primeFd;

	LOG("-- Initializing DRM framebuffer device --\n");

	drmFd = open(DRM_DEVICE, O_RDONLY);
	if (drmFd == -1) {
		LOG(" Cannot open DRM framebuffer '%s'.\n", DRM_DEVICE);
		return -1; // Return to the selector
	} else {
		LOG(" DRM framebuffer '%s' opened successfully.\n", DRM_DEVICE);
	}

	if (drmDropMaster(drmFd) != 0) {
		if (errno != EPERM && errno != EINVAL) {
			LOG(" Failed to drop DRM master: %s\n", strerror(errno));
			close(drmFd);
			exit(EXIT_FAILURE);
		} else {
			LOG(" DRM master not owned, drop not required.\n");
		}
	} else {
		LOG(" DRM master dropped successfully.\n");
	}

	drm_findActiveCrtc();
	drmModeCrtc *crtc = drmModeGetCrtc(drmFd, crtcId);
	if (!crtc) {
		LOG(" Failed to query CRTC: %u\n", crtcId);
		exit(EXIT_FAILURE);
	} else {
		LOG(" Active CRTC: %u.\n", crtcId);
	}

	screenInfo.width = crtc->mode.hdisplay;
	screenInfo.height = crtc->mode.vdisplay;

	drmModeFB2 *buffer = drmModeGetFB2(drmFd, crtc->buffer_id);
	if (!buffer) {
		LOG(" Failed to query framebuffer object: %u.\n", crtc->buffer_id);
		drmModeFreeCrtc(crtc);
		exit(EXIT_FAILURE);
	} else {
		LOG(" Framebuffer object: %u.\n", crtc->buffer_id);
	}

	bufferId = buffer->fb_id;

	if (drmPrimeHandleToFD(drmFd, buffer->handles[0], DRM_CLOEXEC | DRM_RDWR, &primeFd) != 0) {
		LOG(" Failed to create PRIME fd from GEM handle: %u.\n", buffer->handles[0]);
		drmModeFreeFB2(buffer);
		drmModeFreeCrtc(crtc);
		exit(EXIT_FAILURE);
	} else {
		LOG(" PRIME fd created from GEM handle: %u.\n", buffer->handles[0]);
	}

	screenInfo.stride = buffer->pitches[0];

	pixelFormat = buffer->pixel_format;

	// DRM debug information
	LOG(" Screen width: %d px, height: %d px.\n", screenInfo.width, screenInfo.height);
	LOG(" Stride: %d bytes, FourCC format: %.4s.\n", screenInfo.stride, (char *)&pixelFormat);

	drm_updateScreenFormat();

	drmBufferMap = mmap(NULL, screenInfo.stride * screenInfo.height, PROT_READ, MAP_SHARED, primeFd, 0);
	close(primeFd);

	if (drmBufferMap == MAP_FAILED) {
		LOG(" Failed to map DRM framebuffer memory into userspace.\n");
		drmModeFreeFB2(buffer);
		drmModeFreeCrtc(crtc);
		exit(EXIT_FAILURE);
	}

	drmModeFreeFB2(buffer);
	drmModeFreeCrtc(crtc);

	return 0;
}

void drm_closeFrameBuffer(void) {
	if (drmBufferMap != MAP_FAILED)
		munmap(drmBufferMap, screenInfo.stride * screenInfo.height);

	if (drmFd != -1)
		close(drmFd);

	// Reset all framebuffer values
	drmBufferMap = MAP_FAILED;
	drmFd = -1;
	bufferId = -1;
	crtcId = -1;

	LOG(" DRM framebuffer '%s' closed.\n", DRM_DEVICE);
}

void drm_updateFrameBufferInfo(void) {
	drmModeCrtc *crtc = drmModeGetCrtc(drmFd, crtcId);
	if (!crtc) {
		LOG(" Failed to query CRTC state: %u. DRM state lost.\n", crtcId);
		exit(EXIT_FAILURE);
	}

	screenInfo.width = crtc->mode.hdisplay;
	screenInfo.height = crtc->mode.vdisplay;

	if (crtc->buffer_id != bufferId) {
		drmModeFB2 *buffer = drmModeGetFB2(drmFd, crtc->buffer_id);
		if (!buffer) {
			LOG(" Failed to query framebuffer object: %u. DRM state lost.\n", crtc->buffer_id);
			drmModeFreeCrtc(crtc);
			exit(EXIT_FAILURE);
		}

		screenInfo.stride = buffer->pitches[0];

		bufferId = buffer->fb_id;
		pixelFormat = buffer->pixel_format;

		drm_updateScreenFormat();
		drmModeFreeFB2(buffer);
	}

	drmModeFreeCrtc(crtc);
}

int drm_checkBufferStateChange(void) {
	drmModeCrtc *crtc;
	drmModeFB2 *buffer;

	crtc = drmModeGetCrtc(drmFd, crtcId);
	if (!crtc) {
		LOG(" Failed to query CRTC state: %u.\n", crtcId);
		return 1;
	}

	if (crtc->buffer_id == 0) {
		LOG(" No active framebuffer.\n");
		drmModeFreeCrtc(crtc);
		return 1;
	}

	buffer = drmModeGetFB2(drmFd, crtc->buffer_id);
	if (!buffer) {
		LOG(" Failed to query framebuffer object: %u.\n", crtc->buffer_id);
		drmModeFreeCrtc(crtc);
		return 1;
	}

	if (crtc->mode.hdisplay != screenFormat.width ||
	    crtc->mode.vdisplay != screenFormat.height ||
	    buffer->pitches[0] != screenInfo.stride ||
	    buffer->pixel_format != pixelFormat ||
	    buffer->fb_id != bufferId) {

		LOG(" DRM framebuffer state changed.\n");

		drmModeFreeFB2(buffer);
		drmModeFreeCrtc(crtc);
		return 1;
	}

	drmModeFreeFB2(buffer);
	drmModeFreeCrtc(crtc);

	return 0;
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
	return (uint32_t *)drmBufferMap;
}
