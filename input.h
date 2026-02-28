// SPDX-License-Identifier: LGPL-3.0-or-later
// Header file for virtual input devices

#ifndef INPUT_H
#define INPUT_H

#include "common.h"
#include "framebuffer.h"

#include <linux/input.h>
#include <linux/uinput.h>

#include <rfb/keysym.h>

#define BTN_LEFT_MASK 0x1
#define BTN_MIDDLE_MASK 0x2
#define BTN_RIGHT_MASK 0x4
#define WHEEL_UP_MASK 0x8
#define WHEEL_DOWN_MASK 0x10

void initVirtualKeyboard(void);
void initVirtualPointer(void);
void closeVirtualKeyboard(void);
void closeVirtualPointer(void);
void writeEvent(int udev, uint16_t type, uint16_t code, int value);
int keySym2Scancode(rfbKeySym key);
void addKeyboardEvent(rfbBool down, rfbKeySym key, rfbClientPtr cl);
void addPointerEvent(int buttonMask, int x, int y, rfbClientPtr cl);

#endif
