// SPDX-License-Identifier: LGPL-3.0-or-later
// Shared definitions and project-wide includes

#ifndef COMMON_H
#define COMMON_H

#include <dirent.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <sys/stat.h>
#include <sys/sysmacros.h>

#include <fcntl.h>
#include <linux/fb.h>
#include <linux/input.h>

#include <assert.h>
#include <errno.h>
#include <limits.h>

#include <rfb/rfb.h>

#define BPP 32
#define LOG(fmt, ...) do { fprintf(stderr, fmt, ##__VA_ARGS__); } while (0)

#endif
