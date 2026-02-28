// SPDX-License-Identifier: LGPL-3.0-or-later
// Framebuffer backend abstraction

#include "framebuffer.h"

screeninfo_t screenInfo;
screenformat screenFormat;

int activeBackend = BACKEND_NONE;

int initFrameBuffer(void) {

#ifdef HAVE_LIBDRM
	// 1st probe: DRM
	if (drm_initFrameBuffer() == 0) {
		activeBackend = BACKEND_DRM;
		return BACKEND_DRM;
	}
#endif

	// 2nd probe: FBDEV
	if (fbdev_initFrameBuffer() == 0) {
		activeBackend = BACKEND_FBDEV;
		return BACKEND_FBDEV;
	}

	LOG(" There is no backend device available.\n");
	exit(EXIT_FAILURE);
}

void closeFrameBuffer(void) {
	switch (activeBackend) {

#ifdef HAVE_LIBDRM
	case BACKEND_DRM:
		drm_closeFrameBuffer();
		break;
#endif

	case BACKEND_FBDEV:
		fbdev_closeFrameBuffer();
		break;

	case BACKEND_NONE:
	default:
		LOG(" Invalid backend state: %d\n", activeBackend);
		exit(EXIT_FAILURE);
	}
}

int checkResolutionChange(void) {
	switch (activeBackend) {

#ifdef HAVE_LIBDRM
	case BACKEND_DRM:
		return drm_checkResolutionChange();
#endif

	case BACKEND_FBDEV:
		return fbdev_checkResolutionChange();

	case BACKEND_NONE:
	default:
		LOG(" Invalid backend state: %d\n", activeBackend);
		exit(EXIT_FAILURE);
		return -1;
	}
}

uint32_t *readFrameBuffer(void) {
	switch (activeBackend) {

#ifdef HAVE_LIBDRM
	case BACKEND_DRM:
		return drm_readFrameBuffer();
#endif

	case BACKEND_FBDEV:
		return fbdev_readFrameBuffer();

	case BACKEND_NONE:
	default:
		LOG(" Invalid backend state: %d\n", activeBackend);
		exit(EXIT_FAILURE);
		return NULL;
	}
}
