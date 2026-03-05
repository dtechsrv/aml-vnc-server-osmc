// SPDX-License-Identifier: LGPL-3.0-or-later
// FBDEV backend implementation

#include "fbdev.h"

int fbFd = -1;
void *fbBufferMap = MAP_FAILED;
struct fb_var_screeninfo varInfo;

int fbdev_initFrameBuffer(void) {
	size_t fbSize;

	LOG("-- Initializing FBDEV framebuffer device --\n");

	fbFd = open(FB_DEVICE, O_RDONLY);
	if (fbFd == -1) {
		LOG(" Cannot open FBDEV framebuffer device '%s'.\n", FB_DEVICE);
		return -1; // Return to the selector
	} else {
		LOG(" The FBDEV framebuffer device has been attached.\n");
	}

	fbdev_updateFrameBufferInfo();

	if (varInfo.bits_per_pixel != BPP) {
		LOG(" Unsupported BPP value: %u, only %d bit mode supported.\n", varInfo.bits_per_pixel, BPP);
		exit(EXIT_FAILURE);
	}

	fbSize = screenInfo.stride * screenInfo.height;

	fbdev_updateScreenFormat();

	// Framebuffer debug information
	LOG(" Virtual width: %u, virtual height: %u.\n",
		varInfo.xres_virtual, varInfo.yres_virtual);
	LOG(" X axis offset: %u, Y axis offset: %u.\n",
		varInfo.xoffset, varInfo.yoffset);
	LOG(" Stride: %u bytes, framebuffer size: %zu bytes.\n",
		screenInfo.stride, fbSize);

	fbBufferMap = mmap(NULL, fbSize, PROT_READ, MAP_SHARED, fbFd, 0);

	if (fbBufferMap == MAP_FAILED) {
		LOG(" Failed to map FBDEV framebuffer memory into userspace.\n");
		exit(EXIT_FAILURE);
	}

	return 0;
}

void fbdev_closeFrameBuffer(void) {
	if (fbBufferMap != MAP_FAILED)
		munmap(fbBufferMap, screenInfo.stride * screenInfo.height);

	if (fbFd != -1)
		close(fbFd);

	// Reset all framebuffer values
	fbBufferMap = MAP_FAILED;
	fbFd = -1;

	LOG(" The FBDEV framebuffer device has been detached.\n");
}

void fbdev_updateFrameBufferInfo(void) {
	if (ioctl(fbFd, FBIOGET_VSCREENINFO, &varInfo) != 0) {
		LOG(" Failed to query framebuffer screen information.\n");
		exit(EXIT_FAILURE);
	}

	screenInfo.width	= varInfo.xres;
	screenInfo.height	= varInfo.yres;
	screenInfo.stride	= varInfo.xres_virtual * (varInfo.bits_per_pixel / CHAR_BIT);
}

int fbdev_checkBufferStateChange(void) {
	// In the case of FBDEV, the only hard trigger event is the resolution change
	if ((varInfo.xres != screenFormat.width) || (varInfo.yres != screenFormat.height)) {
		LOG("-- Screen resoulution changed from %ux%u to %ux%u --\n",
			screenFormat.width, screenFormat.height,
			varInfo.xres, varInfo.yres);
		fbdev_updateScreenFormat();
		return 1;
	} else {
		return 0;
	}
}

void fbdev_updateScreenFormat(void) {
	screenFormat.width = varInfo.xres;
	screenFormat.height = varInfo.yres;
	screenFormat.bitsPerPixel = varInfo.bits_per_pixel;
	screenFormat.size = screenFormat.width * screenFormat.height * screenFormat.bitsPerPixel / CHAR_BIT;
	screenFormat.redShift = varInfo.red.offset;
	screenFormat.redMax = varInfo.red.length;
	screenFormat.greenShift = varInfo.green.offset;
	screenFormat.greenMax = varInfo.green.length;
	screenFormat.blueShift = varInfo.blue.offset;
	screenFormat.blueMax = varInfo.blue.length;
}

uint32_t *fbdev_readFrameBuffer(void) {
	fbdev_updateFrameBufferInfo();
	return (uint32_t *)fbBufferMap;
}
