// SPDX-License-Identifier: LGPL-3.0-or-later
// FBDEV backend implementation

#include "fbdev.h"

int fb_fd = -1;
void *fb_mmap = MAP_FAILED;
struct fb_var_screeninfo varInfo;

int fbdev_initFrameBuffer(void) {
	size_t fbSize;

	LOG("-- Initializing FBDEV framebuffer device --\n");

	fb_fd = open(FB_DEVICE, O_RDONLY);
	if (fb_fd == -1) {
		LOG(" Cannot open FBDEV framebuffer device '%s'.\n", FB_DEVICE);
		return -1; // Return to the selector
	} else {
		LOG(" The FBDEV framebuffer device has been attached.\n");
	}

	if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &varInfo) != 0) {
		LOG(" FBIOGET_VSCREENINFO failed.\n");
		exit(EXIT_FAILURE);
	}

	if (varInfo.bits_per_pixel != BPP) {
		LOG(" Unsupported BPP value: %u, only %d bit mode supported.\n", varInfo.bits_per_pixel, BPP);
		exit(EXIT_FAILURE);
	}

	screenInfo.width	= varInfo.xres;
	screenInfo.height	= varInfo.yres;
	screenInfo.stride	= varInfo.xres_virtual * (varInfo.bits_per_pixel / CHAR_BIT);

	fbSize = screenInfo.stride * screenInfo.height;

	fbdev_updateScreenFormat();

	// Framebuffer debug information
	LOG(" Virtual width: %u, virtual height: %u.\n",
		varInfo.xres_virtual, varInfo.yres_virtual);
	LOG(" X axis offset: %u, Y axis offset: %u.\n",
		varInfo.xoffset, varInfo.yoffset);
	LOG(" Stride: %u bytes, framebuffer size: %zu bytes.\n",
		screenInfo.stride, fbSize);

	fb_mmap = mmap(NULL, fbSize, PROT_READ, MAP_SHARED, fb_fd, 0);

	if (fb_mmap == MAP_FAILED) {
		LOG(" mmap of FBDEV framebuffer failed.\n");
		exit(EXIT_FAILURE);
	}

	return 0;
}

void fbdev_closeFrameBuffer(void) {
	if (fb_mmap != MAP_FAILED)
		munmap(fb_mmap, screenInfo.stride * screenInfo.height);

	if (fb_fd != -1)
		close(fb_fd);

	// Reset all framebuffer values
	fb_mmap = MAP_FAILED;
	fb_fd = -1;

	LOG(" The FBDEV framebuffer device has been detached.\n");
}

void fbdev_updateFrameBufferInfo(void) {
	if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &varInfo) != 0) {
		LOG(" FBIOGET_VSCREENINFO failed.\n");
		exit(EXIT_FAILURE);
	}

	screenInfo.width	= varInfo.xres;
	screenInfo.height	= varInfo.yres;
	screenInfo.stride	= varInfo.xres_virtual * (varInfo.bits_per_pixel / CHAR_BIT);
}

int fbdev_checkResolutionChange(void) {
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
	return (uint32_t *)fb_mmap;
}
