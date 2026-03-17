// SPDX-License-Identifier: LGPL-3.0-or-later
// Framebuffer backend abstraction

#include "framebuffer.h"

screen_info_t screenInfo;
screen_format_t screenFormat;

int activeBackend = BACKEND_NONE;
int reinitDelay = 0;

void initFrameBuffer(void) {

#ifdef HAVE_LIBDRM
	// 1st probe: DRM
	if (activeBackend == BACKEND_NONE ||
	    activeBackend == BACKEND_DRM) {
		if (drm_initFrameBuffer() == 0) {
			reinitDelay = DRM_DELAY;
			if (activeBackend == BACKEND_NONE)
				activeBackend = BACKEND_DRM;
		}
	}
#endif

	// 2nd probe: FBDEV
	if (activeBackend == BACKEND_NONE ||
	    activeBackend == BACKEND_FBDEV) {
		if (fbdev_initFrameBuffer() == 0) {
			reinitDelay = FB_DELAY;
			if (activeBackend == BACKEND_NONE)
				activeBackend = BACKEND_FBDEV;
		}
	}

	if (activeBackend == BACKEND_NONE) {
		LOG(" There is no backend device available.\n");
		exit(EXIT_FAILURE);
	}
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

int checkBufferStateChange() {
	switch (activeBackend) {

#ifdef HAVE_LIBDRM
	case BACKEND_DRM:
		return drm_checkBufferStateChange();
#endif

	case BACKEND_FBDEV:
		return fbdev_checkBufferStateChange();

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
