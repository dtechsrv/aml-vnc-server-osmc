// SPDX-License-Identifier: LGPL-3.0-or-later
// Header file for the framebuffer backend

#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "common.h"

#include <limits.h>

#define FB_DEVICE "/dev/fb0"

extern struct fb_var_screeninfo screenInfo;

void updateFrameBufferInfo(void);
int roundUpToPageSize(int x);
int initFrameBuffer(void);
void closeFrameBuffer(void);
int checkResolutionChange(void);
void updateScreenFormat(void);
struct fb_var_screeninfo getScreenInfo(void);
unsigned int *readFrameBuffer(void);

#endif
