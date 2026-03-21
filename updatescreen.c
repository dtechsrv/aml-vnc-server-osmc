// SPDX-License-Identifier: LGPL-3.0-or-later
// Screen diff detection and update logic

#include "updatescreen.h"

uint32_t *vncBuffer;

rfbScreenInfoPtr vncScreen;

int updateScreen(void) {
	int x, y, xMin, yMin, xMax, yMax;
	int slip, step, shift, idle;
	int vbOffset = 0, fbOffset = 0, pxOffset = 0;

	// Reset idle state
	idle = 1;

	// Bounding box init
	xMin = 0;
	xMax = screenInfo.width - 1;
	yMin = screenInfo.height - 1;
	yMax = 0;

	// Create buffers
	uint32_t* fb = readFrameBuffer();
	uint32_t* vb = vncBuffer;

	// Set the pixel grid slip (depends on the resolution)
	if (screenInfo.height < 540) {
		slip = 2; // Height below 540 pixels
	} else if (screenInfo.height < 720) {
		slip = 3; // Height between 540 and 719 pixels
	} else if (screenInfo.height < 1080) {
		slip = 4; // Height between 720 and 1079 pixels
	} else if (screenInfo.height < 1440) {
		slip = 5; // Height between 1080 and 1439 pixels
	} else {
		slip = 6; // Height from 1440 pixels and above
	}

	// Set the inline pixel step
	step = SQUARE(slip) - 1;

	// Generate a random step shift (It helps to eliminate any remaining dirty zones between each image update.)
	shift = rand() % step;

	// Compare the buffers and find the differences in every line
	for (y = 0; y < screenInfo.height; y++) {
		// Set all offsets
		vbOffset = y * screenInfo.width;
		fbOffset = (screenInfo.start + y) * (screenInfo.stride / (BPP / CHAR_BIT));
		pxOffset = (y * slip + shift) % step;

		// Compare certain pixels in every line with an offset
		for (x = pxOffset; x < screenInfo.width; x += step) {
			if (vb[x + vbOffset] != fb[x + fbOffset]) {
				if (idle) {
					// The current line reduced by the slip value -> Set as the first different line
					yMin = MIN(y - slip, yMin);
					idle = 0;
				}
				// The current line increased by the slip value -> Set as the last different line
				yMax = MAX(y + slip, yMax);
				break; // There is no need to examine this line anymore if it already has a difference
			}
		}
	}

	// Fill the image buffer with the new content
	if (!idle) {
		yMin = MAX(0, yMin);
		yMax = MIN(screenInfo.height - 1, yMax);

		for (y = yMin; y <= yMax; y++) {
			vbOffset = y * screenInfo.width;
			fbOffset = (screenInfo.start + y) * (screenInfo.stride / (BPP / CHAR_BIT));
			memcpy(vncBuffer + vbOffset, fb + fbOffset, screenInfo.width * BPP / CHAR_BIT);
		}

		rfbMarkRectAsModified(vncScreen, xMin, yMin, xMax, yMax);
	}

	return idle;
}
