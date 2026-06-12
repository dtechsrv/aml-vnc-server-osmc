// SPDX-License-Identifier: LGPL-3.0-or-later
// Header file for the DRM backend

#ifndef DRM_H
#define DRM_H

#include "common.h"
#include "framebuffer.h"

#if __has_include(<libdrm/drm.h>)
#  include <libdrm/drm.h>
#  include <libdrm/drm_fourcc.h>
#else
#  include <drm/drm.h>
#  include <drm/drm_fourcc.h>
#endif

#include <xf86drm.h>
#include <xf86drmMode.h>

#define DRM_DEVICE "/dev/dri/card0"
#define DRM_DELAY 500
#define DRM_FBMAX 4

typedef struct {
    uint32_t fbId;
    uint32_t stride;
    uint32_t pixelFormat;
    uint32_t modeWidth;
    uint32_t modeHeight;
    double refreshRate;
    int multiBuffer;
    int scanFactor;
} drm_state_t;

extern drm_state_t drmState;

void drm_findActiveCrtc(void);
double drm_getFracRate(void);
uint32_t drm_findVideoPlane(void);
int drm_initFrameBuffer(void);
void *drm_mapFrameBuffer(drmModeFB2 *buffer);
void drm_closeFrameBuffer(void);
int drm_checkBufferStateChange(void);
void drm_updateScreenFormat(void);
uint32_t *drm_readFrameBuffer(void);

#endif
