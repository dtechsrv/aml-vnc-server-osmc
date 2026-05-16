// SPDX-License-Identifier: LGPL-3.0-or-later
// DRM backend implementation

#include "drm.h"

int drmFd = -1;
int crtcId = -1;
int fbIdPri, fbIdSec;
void *drmBufferMap, *drmBufferMapPri, *drmBufferMapSec;
drm_state_t drmState;

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
	}

	enc = drmModeGetEncoder(drmFd, conn->encoder_id);
	if (!enc) {
		LOG(" Failed to query encoder: %u.\n", conn->encoder_id);
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
	int refreshRate;

	LOG("-- Initializing DRM framebuffer device --\n");

	drmFd = open(DRM_DEVICE, O_RDONLY);
	if (drmFd == -1) {
		LOG(" Cannot open DRM framebuffer '%s'.\n", DRM_DEVICE);
		return -1; // Return to the selector
	} else {
		LOG(" DRM framebuffer '%s' opened.\n", DRM_DEVICE);
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
		LOG(" DRM master dropped.\n");
	}

	drm_findActiveCrtc();
	drmModeCrtc *crtc = drmModeGetCrtc(drmFd, crtcId);
	if (!crtc) {
		LOG(" Failed to query CRTC state: %u\n", crtcId);
		exit(EXIT_FAILURE);
	}

	drmState.modeWidth = crtc->mode.hdisplay;
	drmState.modeHeight = crtc->mode.vdisplay;
	drmState.modeClock = crtc->mode.clock;
	refreshRate = (crtc->mode.clock * 1000) / (crtc->mode.htotal * crtc->mode.vtotal);

	drmModeFB2 *buffer = drmModeGetFB2(drmFd, crtc->buffer_id);
	if (!buffer) {
		LOG(" Failed to query active framebuffer: %u.\n", crtc->buffer_id);
		drmModeFreeCrtc(crtc);
		exit(EXIT_FAILURE);
	}

	// Checking for multiple buffer usage
	drmState.multiBuffer = buffer->height / (buffer->width * drmState.modeHeight / drmState.modeWidth);

	screenInfo.width = buffer->width;
	screenInfo.height = buffer->height / drmState.multiBuffer;
	screenInfo.stride = buffer->pitches[0];
	screenInfo.start = 0; // On DRM, there is no offset value to extract, so the start value will always be zero.
	drmState.pixelFormat = buffer->pixel_format;
	drmState.fbId = buffer->fb_id;

	fbIdPri = drmState.fbId;
	fbIdSec = 0; // Set default value

	// DRM debug information
	LOG(" Primary framebuffer detected: %d.\n", fbIdPri);
	LOG(" Real screen mode: %dx%d @ %d Hz.\n", drmState.modeWidth, drmState.modeHeight, refreshRate);
	LOG(" Ratio of framebuffer size to actual screen size: %d:1.\n", drmState.multiBuffer);
	LOG(" Used framebuffer width: %d px, height: %d px.\n", screenInfo.width, screenInfo.height);
	LOG(" Stride: %d bytes, FourCC format: %.4s.\n", screenInfo.stride, (char *)&drmState.pixelFormat);

	drm_updateScreenFormat();
	drmBufferMapPri = drm_mapFrameBuffer(buffer);

	if (drmBufferMapPri == MAP_FAILED) {
		LOG(" Failed to map primary DRM framebuffer memory into userspace.\n");
		drmModeFreeFB2(buffer);
		drmModeFreeCrtc(crtc);
		exit(EXIT_FAILURE);
	}

	// Set the primary framebuffer as active
	drmBufferMap = drmBufferMapPri;

	drmModeFreeFB2(buffer);
	drmModeFreeCrtc(crtc);

	return 0;
}

void *drm_mapFrameBuffer(drmModeFB2 *buffer) {
	int primeFd;
	void *bufferMap = MAP_FAILED;

	if (drmPrimeHandleToFD(drmFd, buffer->handles[0], DRM_CLOEXEC | DRM_RDWR, &primeFd) != 0) {
		LOG(" Failed to create PRIME fd from GEM handle: %u.\n", buffer->handles[0]);
		exit(EXIT_FAILURE);
	}

	bufferMap = mmap(NULL, buffer->pitches[0] * buffer->height, PROT_READ, MAP_SHARED, primeFd, 0);
	close(primeFd);

	return bufferMap;
}

void drm_closeFrameBuffer(void) {
	if (drmBufferMapPri != MAP_FAILED)
		munmap(drmBufferMapPri, screenInfo.stride * screenInfo.height * drmState.multiBuffer);

	if (fbIdSec != 0 && drmBufferMapSec != MAP_FAILED)
		munmap(drmBufferMapSec, screenInfo.stride * screenInfo.height * drmState.multiBuffer);

	if (drmFd != -1)
		close(drmFd);

	// Reset all framebuffer values
	drmFd = -1;
	crtcId = -1;

	LOG(" DRM framebuffer '%s' closed.\n", DRM_DEVICE);
}

int drm_checkBufferStateChange(void) {
	drmModeCrtc *crtc;
	drmModeFB2 *buffer;
	int softReinit = 0;

	// Reset DRM reinit delay
	if (reinitDelay != DRM_DELAY)
		reinitDelay = DRM_DELAY;

	// Critical hard reinit triggers
	crtc = drmModeGetCrtc(drmFd, crtcId);
	if (!crtc) {
		LOG(" Failed to query CRTC state: %u.\n", crtcId);
		return 1;
	}

	// This is a standard framebuffer change indicator, if the buffer ID value is temporarily 0
	if (crtc->buffer_id == 0) {
		drmModeFreeCrtc(crtc);

		// Set soft reinit trigger
		softReinit = 1;

		// Retry once after delay
		LOG(" No active framebuffer, retrying with delay.\n");
		if (reinitDelay > 0) {
			usleep(reinitDelay * 1000);
			// This indicates that the delay was already in use, so it is no longer needed later
			reinitDelay = 0;
		}

		crtc = drmModeGetCrtc(drmFd, crtcId);
		if (!crtc) {
			LOG(" Failed to query CRTC state, DRM state lost.\n");
			return 1;
		}

		if (crtc->buffer_id == 0) {
			LOG(" No active framebuffer after retry either, DRM state lost.\n");
			drmModeFreeCrtc(crtc);
			return 1;
		}
	}

	buffer = drmModeGetFB2(drmFd, crtc->buffer_id);
	if (!buffer) {
		LOG(" Failed to query active framebuffer, DRM state lost.\n");
		drmModeFreeCrtc(crtc);
		return 1;
	}

	// Framebuffer ID change handling
	if (buffer->fb_id != drmState.fbId) {
		if (buffer->fb_id == fbIdPri) {
			// Set the primary framebuffer as active
			drmBufferMap = drmBufferMapPri;
		} else if (buffer->fb_id == fbIdSec) {
			// Set the secondary framebuffer as active
			drmBufferMap = drmBufferMapSec;
		} else if (fbIdSec == 0) {
			fbIdSec = buffer->fb_id;
			LOG(" Secondary framebuffer detected: %d.\n", fbIdSec);

			// Init the secondary framebuffer
			drmBufferMapSec = drm_mapFrameBuffer(buffer);

			if (drmBufferMapSec == MAP_FAILED) {
				LOG(" Failed to map secondary DRM framebuffer memory into userspace.\n");
				drmModeFreeFB2(buffer);
				drmModeFreeCrtc(crtc);
				return 1;
			}

			// Set the secondary framebuffer as active
			drmBufferMap = drmBufferMapSec;
		} else {
			softReinit = 1; // When a third framebuffer ID appears
		}

		// Set current framebuffer ID
		drmState.fbId = buffer->fb_id;
	}

	// Hard reinit triggers: display resolution width or height
	if (crtc->mode.hdisplay != drmState.modeWidth ||
	    crtc->mode.vdisplay != drmState.modeHeight) {

		LOG(" Screen resolution changed from %ux%u to %ux%u.\n",
			drmState.modeWidth, drmState.modeHeight,
			crtc->mode.hdisplay, crtc->mode.vdisplay);
		drmModeFreeFB2(buffer);
		drmModeFreeCrtc(crtc);
		return 1;
	}

	// Soft reinit triggers: mode clock, pixel format or previously set soft reinit variable
	if (crtc->mode.clock != drmState.modeClock ||
	    buffer->pixel_format != drmState.pixelFormat ||
	    softReinit) {

		LOG("-- DRM framebuffer state changed --\n");
		drmModeFreeFB2(buffer);
		drmModeFreeCrtc(crtc);

		closeFrameBuffer();
		if (reinitDelay > 0)
			usleep(reinitDelay * 1000);
		initFrameBuffer();

		return 0;
	}

	drmModeFreeFB2(buffer);
	drmModeFreeCrtc(crtc);

	return 0;
}

void drm_updateScreenFormat(void) {
	screenFormat.width = screenInfo.width;
	screenFormat.height = screenInfo.height;
	screenFormat.size = screenInfo.width * screenInfo.height * (BPP / CHAR_BIT);

	switch (drmState.pixelFormat) {

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
		LOG(" Unsupported pixel format: 0x%x. Exiting.\n", drmState.pixelFormat);
		exit(EXIT_FAILURE);
	}
}

uint32_t *drm_readFrameBuffer(void) {
	return (uint32_t *)drmBufferMap;
}
