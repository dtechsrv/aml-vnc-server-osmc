// SPDX-License-Identifier: LGPL-3.0-or-later
// Header file for the framebuffer backend

#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#define DRM_DEVICE "/dev/dri/card0"

#include "common.h"

typedef struct {
    int current_fb_id;
    int width;
    int height;
    int stride;
    int pixel_format;
    int crtc_id;
} drm_screeninfo;

extern drm_screeninfo screenInfo;
extern int fb_pixels_per_line;

void findActiveCrtc(void);
void updateFrameBufferInfo(void);
void initFrameBuffer(void);
int checkResolutionChange(void);
void updateScreenFormat(void);
uint32_t *readFrameBuffer(void);
void closeFrameBuffer(void);

#endif
