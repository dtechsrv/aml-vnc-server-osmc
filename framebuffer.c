/*
droid VNC server  - a vnc server for android
Copyright (C) 2011 Jose Pereira <onaips@gmail.com>

Other contributors:
  Oleksandr Andrushchenko <andr2000@gmail.com>

Modified for AML TV Boxes by kszaq <kszaquitto@gmail.com>
Additional developments by dtech(.hu) <dee.gabor@gmail.com>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 3 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "framebuffer.h"

screenformat screenFormat;

int fbfd = -1;
unsigned int *fbmmap;

struct fb_var_screeninfo screenInfo;
struct fb_fix_screeninfo fixScreenInfo;

void updateFrameBufferInfo(void) {
	if (ioctl(fbfd, FBIOGET_VSCREENINFO, &screenInfo) != 0) {
		L(" 'ioctl' error!\n");
		exit(EXIT_FAILURE);
	}
}

int roundUpToPageSize(int x) {
	return (x + (sysconf(_SC_PAGESIZE)-1)) & ~(sysconf(_SC_PAGESIZE)-1);
}

int initFrameBuffer(void) {
	L("-- Initializing framebuffer device --\n");

	fbmmap = MAP_FAILED;

	if ((fbfd = open(FB_DEVICE, O_RDONLY)) == -1) {
		L(" Cannot open framebuffer device '%s'.\n", FB_DEVICE);
		return -1;
	} else {
		L(" The framebuffer device has been attached.\n");
	}

	updateFrameBufferInfo();

	if (ioctl(fbfd, FBIOGET_FSCREENINFO, &fixScreenInfo) != 0) {
		L(" 'ioctl' error!\n");
		return -1;
	}

	// Framebuffer debug information
	L(" Virtual width: %d, virtual height: %d.\n",
		(int)screenInfo.xres_virtual, (int)screenInfo.yres_virtual);
	L(" X axis offset: %d, Y axis offset: %d.\n",
		(int)screenInfo.xoffset, (int)screenInfo.yoffset);

	size_t size = screenInfo.yres_virtual;
	size_t fbSize = roundUpToPageSize(fixScreenInfo.line_length * size);

	L(" Line length: %d bytes, framebuffer size: %d bytes.\n",
		(int)fixScreenInfo.line_length, (int)fbSize);

	fbmmap = mmap(NULL, fbSize, PROT_READ, MAP_SHARED, fbfd, 0);

	if (fbmmap == MAP_FAILED) {
		L(" 'mmap' failed!\n");
		return -1;
	}

	updateScreenFormat();

	return 1;
}

void closeFrameBuffer(void) {
	if(fbfd != -1)
	close(fbfd);
	L(" The framebuffer device has been detached.\n");
}

int checkResolutionChange(void) {
	if ((screenInfo.xres != screenFormat.width) || (screenInfo.yres != screenFormat.height)) {
		L("-- Screen resoulution changed from %dx%d to %dx%d --\n",
			(int)screenFormat.width, (int)screenFormat.height,
			(int)screenInfo.xres, (int)screenInfo.yres);
		updateScreenFormat();
		return 1;
	} else {
		return 0;
	}
}

void updateScreenFormat(void) {
	screenFormat.width = screenInfo.xres;
	screenFormat.height = screenInfo.yres;
	screenFormat.bitsPerPixel = screenInfo.bits_per_pixel;
	screenFormat.size = screenFormat.width * screenFormat.height * screenFormat.bitsPerPixel / CHAR_BIT;
	screenFormat.redShift = screenInfo.red.offset;
	screenFormat.redMax = screenInfo.red.length;
	screenFormat.greenShift = screenInfo.green.offset;
	screenFormat.greenMax = screenInfo.green.length;
	screenFormat.blueShift = screenInfo.blue.offset;
	screenFormat.blueMax = screenInfo.blue.length;
}

struct fb_var_screeninfo getScreenInfo(void) {
	return screenInfo;
}

unsigned int *readFrameBuffer(void) {
	updateFrameBufferInfo();
	return fbmmap;
}
