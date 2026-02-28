// SPDX-License-Identifier: LGPL-3.0-or-later
// Header file for the DRM backend

#ifndef DRM_H
#define DRM_H

#include "common.h"
#include "framebuffer.h"

#include <drm/drm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>

#define DRM_DEVICE "/dev/dri/card0"

void drm_findActiveCrtc(void);
int drm_initFrameBuffer(void);
void drm_closeFrameBuffer(void);
void drm_updateFrameBufferInfo(void);
int drm_checkResolutionChange(void);
void drm_updateScreenFormat(void);
uint32_t *drm_readFrameBuffer(void);

#endif
