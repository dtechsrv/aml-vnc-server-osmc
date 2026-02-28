// SPDX-License-Identifier: LGPL-3.0-or-later
// Header file for the FBDEV backend

#ifndef FBDEV_H
#define FBDEV_H

#include "common.h"
#include "framebuffer.h"

#define FB_DEVICE "/dev/fb0"

int fbdev_initFrameBuffer(void);
void fbdev_closeFrameBuffer(void);
void fbdev_updateFrameBufferInfo(void);
int fbdev_checkResolutionChange(void);
void fbdev_updateScreenFormat(void);
uint32_t *fbdev_readFrameBuffer(void);

#endif
