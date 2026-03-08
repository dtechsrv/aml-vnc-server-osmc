// SPDX-License-Identifier: LGPL-3.0-or-later
// Header file for framebuffer backend abstraction

#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "common.h"

#ifdef HAVE_LIBDRM
#include "backend/drm.h"
#endif

#include "backend/fbdev.h"

#define BACKEND_NONE	0
#define BACKEND_FBDEV	1
#define BACKEND_DRM	2

extern int reinitDelay;

typedef struct {
	uint32_t width;		// Screen width in pixels
	uint32_t height;	// Screen height in pixels
	uint32_t stride;	// Aligned value of bytes per line
} screen_info_t;

extern screen_info_t screenInfo;

typedef struct _screenformat {
	uint16_t width;
	uint16_t height;

	uint8_t bitsPerPixel;

	uint16_t redMax;
	uint16_t greenMax;
	uint16_t blueMax;

	uint8_t redShift;
	uint8_t greenShift;
	uint8_t blueShift;

	uint32_t size;
	uint32_t pad;
} screen_format_t;

extern screen_format_t screenFormat;

int initFrameBuffer(void);
void closeFrameBuffer(void);
int checkBufferStateChange(void);
uint32_t *readFrameBuffer(void);

#endif
