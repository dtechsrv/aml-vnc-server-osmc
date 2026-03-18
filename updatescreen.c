// SPDX-License-Identifier: LGPL-3.0-or-later
// Screen diff detection and update logic

#include "updatescreen.h"

uint32_t *vncBuffer;

rfbScreenInfoPtr vncScreen;

int updateScreen(int width, int height, int bpp) {
	int x, y, slip, step, shift;
	int vbOffset = 0, fbOffset = 0, pxOffset = 0;
	int maxX = -1, maxY = -1, minX = 99999, minY = 99999;
	int idle = 1;

	// Create buffers
	uint32_t* fb = readFrameBuffer();
	uint32_t* vb = vncBuffer;

	// Set the pixel grid slip (depends on the resolution)
	if (height < 540) {
		slip = 2;
	} else if (height >= 540 && height < 720) {
		slip = 3;
	} else if (height >= 720 && height < 1080) {
		slip = 4;
	} else if (height >= 1080 && height < 1440) {
		slip = 5;
	} else {
		slip = 6;
	}

	// Set the inline pixel step
	step = SQUARE(slip) - 1;

	// Generate a random step shift (It helps to eliminate any remaining dirty zones between each image update.)
	shift = rand() % step;

	// Compare the buffers and find the differences in every line
	for (y = 0; y < height; y++) {
		// Set all offsets
		vbOffset = y * width;
		fbOffset = y * (screenInfo.stride / (bpp / CHAR_BIT));
		pxOffset = (y * slip + shift) % step;

		// Compare certain pixels in every line with an offset
		for (x = pxOffset; x < width; x += step) {
			if (vb[x + vbOffset] != fb[x + fbOffset]) {
				if (idle) {
					// The current line reduced by the slip value -> Set as the first different line
					minY = MIN(y - slip, minY);
					idle = 0;
				}
				// The current line increased by the slip value -> Set as the last different line
				maxY = MAX(y + slip, maxY);
				break; // There is no need to examine this line anymore if it already has a difference
			}
		}
	}

	// Fill the image buffer with the new content
	if (!idle) {
		minX = 0;
		minY = MAX(0, minY);
		maxX = width - 1;
		maxY = MIN(height - 1, maxY);

		for (y = minY; y <= maxY; y++) {
			vbOffset = y * width;
			fbOffset = y * (screenInfo.stride / (bpp / CHAR_BIT));
			memcpy(vncBuffer + vbOffset, fb + fbOffset, width * bpp / CHAR_BIT);
		}

		rfbMarkRectAsModified(vncScreen, minX, minY, maxX, maxY);
	}

	return idle;
}
