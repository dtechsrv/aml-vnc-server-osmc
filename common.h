// SPDX-License-Identifier: LGPL-3.0-or-later
// Shared definitions and project-wide includes

#ifndef COMMON_H
#define COMMON_H

#ifndef __cplusplus

#include <dirent.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <sys/stat.h>
#include <sys/sysmacros.h>             /* For makedev() */

#include <fcntl.h>
#include <linux/fb.h>
#include <linux/input.h>

#include <assert.h>
#include <errno.h>

#include <rfb/rfb.h>

#define L(...) do{ printf(__VA_ARGS__); } while (0);
#endif

typedef struct _screenformat {
	uint16_t width;
	uint16_t height;

	uint8_t bitsPerPixel;

	uint16_t redMax;
	uint16_t greenMax;
	uint16_t blueMax;

	uint8_t redShift;
	uint8_t greenShift;
	uint8_t blueShift;

	uint32_t size;
	uint32_t pad;
} screenformat;

extern screenformat screenFormat;

#endif
