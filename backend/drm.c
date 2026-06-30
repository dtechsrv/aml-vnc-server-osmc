// SPDX-License-Identifier: LGPL-3.0-or-later
// DRM backend implementation

#include "drm.h"

int drmFd = -1;
int fbIndex = -1;
int initCount = 0;
uint32_t connId, crtcId, fbId[DRM_FBMAX];
void *drmBufferMap, *drmBufferMapList[DRM_FBMAX];
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

	connId = conn->connector_id;

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

double drm_getFracRate(void) {
	drmModeObjectProperties *connProps;
	drmModePropertyRes *propInfo;
	double value = 1;
	int i;

	connProps = drmModeObjectGetProperties(drmFd, connId, DRM_MODE_OBJECT_CONNECTOR);
	if (!connProps)
		return value;

	for (i = 0; i < connProps->count_props; i++) {
		propInfo = drmModeGetProperty(drmFd, connProps->props[i]);
		if (propInfo && !strcmp(propInfo->name, "FRAC_RATE_POLICY")) {
			if (connProps->prop_values[i])
				value = 1.001;
			drmModeFreeProperty(propInfo);
			break;
		}

		if (propInfo)
			drmModeFreeProperty(propInfo);
	}

	drmModeFreeObjectProperties(connProps);
	return value;
}

uint32_t drm_findVideoPlane(void) {
	drmModePlaneRes *planeRes;
	drmModePlane *plane;
	uint32_t planeId = 0;
	int i;

	planeRes = drmModeGetPlaneResources(drmFd);
	if (!planeRes)
		return 0;

	for (i = 0; i < planeRes->count_planes; i++) {
		plane = drmModeGetPlane(drmFd, planeRes->planes[i]);

		if (!plane)
			continue;

		if (plane->crtc_id == crtcId && plane->fb_id != 0) {
			planeId = plane->plane_id;
			drmModeFreePlane(plane);
			break;
		}

		drmModeFreePlane(plane);
	}

	drmModeFreePlaneResources(planeRes);
	return planeId;
}

int drm_initFrameBuffer(void) {
	LOG("-- Initializing DRM framebuffer device - Count: %d --\n", initCount + 1);

	drmFd = open(DRM_DEVICE, O_RDONLY);
	if (drmFd == -1) {
		LOG(" Cannot open DRM framebuffer '%s'.\n", DRM_DEVICE);
		if (!initCount) {
			return -1; // Return to the selector
		} else {
			exit(EXIT_FAILURE);
		}
	}

	if (drmDropMaster(drmFd) != 0) {
		if (errno != EPERM && errno != EINVAL) {
			LOG(" Failed to drop DRM master: %s\n", strerror(errno));
			close(drmFd);
			exit(EXIT_FAILURE);
		}
	}

	drm_findActiveCrtc();
	drmModeCrtc *crtc = drmModeGetCrtc(drmFd, crtcId);
	if (!crtc) {
		LOG(" Failed to query CRTC state: %u\n", crtcId);
		exit(EXIT_FAILURE);
	}

	drmState.modeWidth = crtc->mode.hdisplay;
	drmState.modeHeight = crtc->mode.vdisplay;
	drmState.scanFactor = (crtc->mode.flags & DRM_MODE_FLAG_INTERLACE) ? 2 : 1;
	drmState.refreshRate = (double)(crtc->mode.clock * 1000 * drmState.scanFactor) /
		(crtc->mode.htotal * crtc->mode.vtotal * drm_getFracRate());

	drmModeFB2 *buffer = drmModeGetFB2(drmFd, crtc->buffer_id);
	if (!buffer) {
		if (drm_findVideoPlane()) {
			LOG(" No framebuffer found, but video plane is active.\n");
			suspend = 1;
		} else {
			LOG(" Failed to query active framebuffer: %u.\n", crtc->buffer_id);
			drmModeFreeCrtc(crtc);
			exit(EXIT_FAILURE);
		}
	} else {
		if (buffer->modifier != DRM_FORMAT_MOD_LINEAR) {
			LOG(" Non-linear framebuffer modifier detected, exiting.\n");
			drmModeFreeFB2(buffer);
			drmModeFreeCrtc(crtc);
			exit(EXIT_FAILURE);
		}
	}

	if (!suspend) {
		// Checking for multiple buffer usage
		drmState.multiBuffer = buffer->height / (buffer->width * drmState.modeHeight / drmState.modeWidth);

		screenInfo.width = buffer->width;
		screenInfo.height = buffer->height / drmState.multiBuffer;
		screenInfo.stride = buffer->pitches[0];
		screenInfo.start = 0; // On DRM, there is no offset value to extract, so the start value will always be zero.
		drmState.pixelFormat = buffer->pixel_format;
		drmState.fbId = buffer->fb_id;

		// Init first DRM framebuffer
		fbIndex = 0;
		fbId[fbIndex] = drmState.fbId;
	} else {
		// Assuming scaling with a height limit of 1080 pixels
		if (drmState.modeHeight > 1080) {
			screenInfo.width = 1920;
			screenInfo.height = 1080;
		} else {
			screenInfo.width = drmState.modeWidth;
			screenInfo.height = drmState.modeHeight;
		}

		// Set default values, because there are no values to query
		screenInfo.stride = screenInfo.width * (BPP / CHAR_BIT);
		screenInfo.start = 0;
		drmState.multiBuffer = 1;
		drmState.pixelFormat = DRM_FORMAT_ARGB8888;
		drmState.fbId = 0;
	}

	drmState.colorGroup = drm_updateScreenFormat();
	if (!drmState.colorGroup) {
		LOG(" Unsupported pixel format: 0x%x, exiting.\n", drmState.pixelFormat);
		if (!suspend)
			drmModeFreeFB2(buffer);
		drmModeFreeCrtc(crtc);
		exit(EXIT_FAILURE);
	}

	// DRM debug information
	LOG(" Real screen mode: %ux%u%c @ %.2f Hz.\n", drmState.modeWidth,
		drmState.modeHeight, drmState.scanFactor == 2 ? 'i' : 'p', drmState.refreshRate);
	LOG(" Used framebuffer width: %d px, height: %d px.\n", screenInfo.width, screenInfo.height);
	LOG(" Stride: %d bytes, FourCC format: %.4s.\n", screenInfo.stride, (char *)&drmState.pixelFormat);

	if (!suspend) {
		LOG(" Initial DRM framebuffer detected (#%d): %u.\n", fbIndex + 1, fbId[fbIndex]);
		LOG(" Ratio of framebuffer size to actual screen size: %d:1.\n", drmState.multiBuffer);
	}

	if (!suspend) {
		drmBufferMapList[fbIndex] = drm_mapFrameBuffer(buffer);

		if (drmBufferMapList[fbIndex] == MAP_FAILED) {
			LOG(" Failed to map primary DRM framebuffer memory into userspace.\n");
			drmModeFreeFB2(buffer);
			drmModeFreeCrtc(crtc);
			exit(EXIT_FAILURE);
		}

		// Set first framebuffer as active
		drmBufferMap = drmBufferMapList[fbIndex];
		drmModeFreeFB2(buffer);
	}

	drmModeFreeCrtc(crtc);

	// Increase init counter
	initCount++;

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
	int i;

	for (i = 0; i <= fbIndex && i < DRM_FBMAX; i++) {
		if (drmBufferMapList[i] != MAP_FAILED)
			munmap(drmBufferMapList[i], screenInfo.stride * screenInfo.height * drmState.multiBuffer);
	}

	close(drmFd);

	// Reset all framebuffer values
	drmFd = -1;
	fbIndex = -1;
}

int drm_checkBufferStateChange(void) {
	drmModeCrtc *crtc;
	drmModeFB2 *buffer;
	double refreshRate;
	int fbActive = -1;
	int softReinit = 0;
	int multiBuffer = 1;
	int scanFactor, colorGroup, i;

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
		if (!suspend) {
			drmModeFreeCrtc(crtc);

			if (reinitDelay > 0) {
				LOG(" No active framebuffer found, retry after %d ms delay.\n", reinitDelay);
				usleep(reinitDelay * 1000);
				reinitDelay = 0; // This indicates that the delay was already in use, so it is no longer needed later
			}

			crtc = drmModeGetCrtc(drmFd, crtcId);
			if (!crtc) {
				LOG(" Failed to query CRTC state, DRM state lost.\n");
				return 1;
			}

			if (crtc->buffer_id == 0) {
				if (drm_findVideoPlane()) {
					LOG(" The video plane is active, suspended state is initiated.\n");
					suspend = 1;
					return 0;
				} else {
					LOG(" There is still no framebuffer or active video plane.\n");
					drmModeFreeCrtc(crtc);
					return 1;
				}
			}

			softReinit = 1; // If it was 0 due to a state change, then a soft reinit is definitely required
		} else {
			drmModeFreeCrtc(crtc);
			return 0; // The suspended state is still active, no further verification is required in this cycle
		}
	} else {
		if (suspend) {
			suspend = 0; // The post-suspension check will determines the reinit level, whether it is hard or soft
			if (fbIndex >= 0)
				LOG(" Active framebuffer found again, returning from suspended state.\n");
		}
	}

	// Skipping the complete soft/hard verification chain when buffer suspension is active
	if (!suspend) {
		buffer = drmModeGetFB2(drmFd, crtc->buffer_id);
		if (!buffer) {
			LOG(" Failed to query active framebuffer, DRM state lost.\n");
			drmModeFreeCrtc(crtc);
			return 1;
		}

		// Scan mode change
		scanFactor = (crtc->mode.flags & DRM_MODE_FLAG_INTERLACE) ? 2 : 1;
		if (scanFactor != drmState.scanFactor) {
			LOG(" Scan mode changed from %s to %s.\n",
				drmState.scanFactor == 2 ? "interlaced" : "progressive",
				scanFactor == 2 ? "interlaced" : "progressive");
				softReinit = 1;
		}

		// Refresh rate change
		refreshRate = (double)(crtc->mode.clock * 1000 * scanFactor) / (crtc->mode.htotal * crtc->mode.vtotal * drm_getFracRate());
		if (refreshRate != drmState.refreshRate) {
			LOG(" Screen refresh rate changed from %.2f Hz to %.2f Hz.\n", drmState.refreshRate, refreshRate);
			softReinit = 1;
		}

		// Pixel format change
		if (buffer->pixel_format != drmState.pixelFormat) {
			LOG(" Screen pixel format changed from %.4s to %.4s.\n", (char *)&drmState.pixelFormat, (char *)&buffer->pixel_format);

			colorGroup = drm_updateScreenFormat();
			if (colorGroup != drmState.colorGroup) {
				drmModeFreeFB2(buffer);
				drmModeFreeCrtc(crtc);
				return 1; // Hard reinit is required because libvncserver does not update color profile during active server session
			} else {
				softReinit = 1;
			}
		}

		// Multibuffer ratio change
		multiBuffer = buffer->height / (buffer->width * crtc->mode.vdisplay / crtc->mode.hdisplay);
		if (multiBuffer != drmState.multiBuffer) {
			LOG(" Ratio of buffer to screen size changed from %d:1 to %d:1.\n", drmState.multiBuffer, multiBuffer);
			softReinit = 1;
		}

		// Framebuffer ID change
		if (buffer->fb_id != drmState.fbId && !softReinit) {

			for (i = 0; i <= fbIndex; i++) {
				if (buffer->fb_id == fbId[i]) {
					fbActive = i;

					// Set current framebuffer ID and memory map pointer as active
					drmState.fbId = fbId[fbActive];
					drmBufferMap = drmBufferMapList[fbActive];

					break;
				}
			}

			// New framebuffer handling
			if (fbActive < 0) {
				fbIndex++;
				if (fbIndex < DRM_FBMAX) {
					fbActive = fbIndex;
					fbId[fbActive] = buffer->fb_id;

					if (fbIndex == 0) {
						LOG(" Initial DRM framebuffer detected (#%d): %u.\n", fbIndex + 1, fbId[fbIndex]);
					} else {
						LOG(" New DRM framebuffer detected (#%d): %u.\n", fbActive + 1, fbId[fbActive]);
					}

					// Set the value of multibuffer ratio if initialization is completed in suspended state
					if (fbIndex == 0) {
						drmState.multiBuffer = buffer->height / (buffer->width * crtc->mode.vdisplay / crtc->mode.hdisplay);
						LOG(" Ratio of framebuffer size to actual screen size: %d:1.\n", drmState.multiBuffer);
					}

					// Init the new framebuffer
					drmBufferMapList[fbActive] = drm_mapFrameBuffer(buffer);

					if (drmBufferMapList[fbActive] == MAP_FAILED) {
						LOG(" Failed to map DRM framebuffer (#%d) memory into userspace.\n", fbActive + 1);
						drmModeFreeFB2(buffer);
						drmModeFreeCrtc(crtc);
						return 1;
					}
				} else {
					softReinit = 1;
				}
			}

			// Set current framebuffer ID and memory map pointer as active
			if (!softReinit) {
				drmState.fbId = fbId[fbActive];
				drmBufferMap = drmBufferMapList[fbActive];
			}
		}

		// Display resolution width or height -> Hard reinit is required because most VNC clients do not handle screen size changes
		if (crtc->mode.hdisplay != drmState.modeWidth ||
		    crtc->mode.vdisplay != drmState.modeHeight) {
			LOG(" Screen resolution changed from %ux%u to %ux%u.\n",
				drmState.modeWidth, drmState.modeHeight,
				crtc->mode.hdisplay, crtc->mode.vdisplay);
			drmModeFreeFB2(buffer);
			drmModeFreeCrtc(crtc);
			return 1;
		}

		// Buffer width and height -> A hard reinit is required because the same policy applies as for resolution
		if (buffer->width != screenInfo.width ||
		    (buffer->height / multiBuffer) != screenInfo.height) {
			LOG(" DRM framebuffer size changed from %ux%u to %ux%u.\n",
				screenInfo.width, screenInfo.height,
				buffer->width, buffer->height);
			drmModeFreeFB2(buffer);
			drmModeFreeCrtc(crtc);
			return 1;
		}

		drmModeFreeFB2(buffer);
	}

	// Perform a soft reinit if trigger is set
	if (softReinit) {
		LOG(" DRM framebuffer state changed, reinitialization started...\n");
		drmModeFreeCrtc(crtc);

		closeFrameBuffer();
		if (reinitDelay > 0)
			usleep(reinitDelay * 1000);
		initFrameBuffer();

		return 0;
	}

	drmModeFreeCrtc(crtc);
	return 0;
}

int drm_updateScreenFormat(void) {
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
		return 1;

	case DRM_FORMAT_ABGR8888:
		screenFormat.bitsPerPixel	= 32;
		screenFormat.redShift		= 0;
		screenFormat.greenShift		= 8;
		screenFormat.blueShift		= 16;
		screenFormat.redMax		= 8;
		screenFormat.greenMax		= 8;
		screenFormat.blueMax		= 8;
		return 2;

	default:
		return 0; // Unsupported pixel format
	}
}

uint32_t *drm_readFrameBuffer(void) {
	return (uint32_t *)drmBufferMap;
}
