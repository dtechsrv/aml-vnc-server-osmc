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
#define DRM_DELAY 1000

typedef struct {
    int fbId;
    int stride;
    int pixelFormat;
    int modeClock;
    int modeWidth;
    int modeHeight;
    int multiBuffer;
} drm_state_t;

extern drm_state_t drmState;

void drm_findActiveCrtc(void);
int drm_initFrameBuffer(void);
void drm_closeFrameBuffer(void);
void drm_updateFrameBufferInfo(void);
int drm_checkBufferStateChange(void);
void drm_updateScreenFormat(void);
uint32_t *drm_readFrameBuffer(void);

#endif
