// SPDX-License-Identifier: LGPL-3.0-or-later
// Header file for screen update and diff logic

#ifndef UPDATESCREEN_H
#define UPDATESCREEN_H

#include "common.h"
#include "framebuffer.h"

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#define SQUARE(x) ((x)*(x))

extern unsigned int *vncBuffer;

extern rfbScreenInfoPtr vncScreen;

int updateScreen(int width, int height, int bpp);

#endif
