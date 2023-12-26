/*
This code comes from:
dispmanx - a VNC server for Raspberry Pi
Copyright (C) 2013 Peter Hanzel <hanzelpeter@gmail.com>

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

#include "newinput.h"

int ukbd, uptr;
bool down_keys[KEY_CNT];
int mouse_x, mouse_y;
int mouse_button = 0;

void initVirtKbd() {
	struct uinput_user_dev uinp;
	int retcode, i;

	memset(down_keys, 0, sizeof(down_keys));

	ukbd = open("/dev/uinput", O_WRONLY | O_NDELAY );
	printf("open /dev/uinput returned %d.\n", ukbd);

	if (ukbd == 0) {
		printf("Could not open uinput.\n");
		exit(-1);
	}

	memset(&uinp, 0, sizeof(uinp));
	strncpy(uinp.name, "VNC virtual keyboard", 20);
	uinp.id.version = 4;
	uinp.id.bustype = BUS_USB;

	ioctl(ukbd, UI_SET_EVBIT, EV_SYN);
	ioctl(ukbd, UI_SET_EVBIT, EV_KEY);

	for (i=0; i<KEY_MAX; i++) {
		ioctl(ukbd, UI_SET_KEYBIT, i);
	}

	retcode = write(ukbd, &uinp, sizeof(uinp));
	printf("First write returned %d.\n", retcode);

	retcode = (ioctl(ukbd, UI_DEV_CREATE));
	printf("ioctl UI_DEV_CREATE returned %d.\n", retcode);

	if (retcode) {
		printf("Error create uinput device %d.\n", retcode);
		exit(-1);
	}
}

void initVirtPtr() {
	struct uinput_user_dev uinp;
	int retcode, i;

	uptr = open("/dev/uinput", O_WRONLY | O_NDELAY );
	printf("open /dev/uinput returned %d.\n", uptr);

	if (uptr == 0) {
		printf("Could not open uinput.\n");
		exit(-1);
	}

	memset(&uinp, 0, sizeof(uinp));
	strncpy(uinp.name, "VNC virtual pointer", 20);
	uinp.id.version = 1;
	uinp.id.bustype = BUS_USB;

	ioctl(uptr, UI_SET_EVBIT, EV_SYN);
	ioctl(uptr, UI_SET_EVBIT, EV_KEY);
	ioctl(uptr, UI_SET_EVBIT, EV_REL);
	ioctl(uptr, UI_SET_EVBIT, EV_ABS);

	ioctl(uptr, UI_SET_KEYBIT, BTN_LEFT);
	ioctl(uptr, UI_SET_KEYBIT, BTN_RIGHT);
	ioctl(uptr, UI_SET_KEYBIT, BTN_MIDDLE);

	ioctl(uptr, UI_SET_RELBIT, REL_WHEEL);

	ioctl(uptr, UI_SET_ABSBIT, ABS_X);
	ioctl(uptr, UI_SET_ABSBIT, ABS_Y);

	uinp.absmin[ABS_X] = 0;
	uinp.absmax[ABS_X] = screenformat.width - 1;
	uinp.absmin[ABS_Y] = 0;
	uinp.absmax[ABS_Y] = screenformat.height - 1;

	retcode = write(uptr, &uinp, sizeof(uinp));
	printf("First write returned %d.\n", retcode);

	retcode = (ioctl(uptr, UI_DEV_CREATE));
	printf("ioctl UI_DEV_CREATE returned %d.\n", retcode);

	if (retcode) {
		printf("Error create uinput device %d.\n", retcode);
		exit(-1);
	}
}

void closeVirtKbd() {
	ioctl(ukbd, UI_DEV_DESTROY);
	close(ukbd);
}

void closeVirtPtr() {
	ioctl(uptr, UI_DEV_DESTROY);
	close(uptr);
}

void writeEvent(int udev, __u16 type, __u16 code, __s32 value) {
	struct input_event event;
	memset(&event, 0, sizeof(event));
	gettimeofday(&event.time, NULL);
	event.type = type;
	event.code = code;
	event.value = value;
	write(udev, &event, sizeof(event));
}

int keysym2scancode(rfbKeySym key) {
	int scancode = 0;
	int code = (int) key;

	//printf("DEBUG -> Keyboard keysim code: %04X.\n", key);

	if (code>='0' && code<='9') {
		scancode = (code & 0xF) - 1;
		if (scancode<0) scancode += 10;
		scancode += KEY_1;
	} else if (code>=0xFF50 && code<=0xFF58) {
		static const uint16_t map[] = {
			KEY_HOME, KEY_LEFT, KEY_UP, KEY_RIGHT, KEY_DOWN,
			KEY_PAGEUP, KEY_PAGEDOWN, KEY_END, 0 };
		scancode = map[code & 0xF];
	} else if (code>=0xFFE1 && code<=0xFFEE) {
		static const uint16_t map[] = {
			KEY_LEFTSHIFT, KEY_LEFTSHIFT,
			KEY_LEFTCTRL, KEY_LEFTCTRL,
			KEY_LEFTSHIFT, KEY_LEFTSHIFT,
			0,0,
			KEY_LEFTALT, KEY_RIGHTALT,
			0, 0, 0, 0 };
		scancode = map[code & 0xF];
	} else if ((code>='A' && code<='Z') || (code>='a' && code<='z')) {
		static const uint16_t map[] = {
			KEY_A, KEY_B, KEY_C, KEY_D, KEY_E,
			KEY_F, KEY_G, KEY_H, KEY_I, KEY_J,
			KEY_K, KEY_L, KEY_M, KEY_N, KEY_O,
			KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T,
			KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z };
		scancode = map[(code & 0x5F) - 'A'];
	} else {
		switch (code) {
			case XK_space: scancode = KEY_SPACE; break;
			case XK_exclam: scancode = KEY_1; break;
			case XK_at: scancode = KEY_2; break;
			case XK_numbersign: scancode = KEY_3; break;
			case XK_dollar: scancode = KEY_4; break;
			case XK_percent: scancode = KEY_5; break;
			case XK_asciicircum: scancode = KEY_6; break;
			case XK_ampersand: scancode = KEY_7; break;
			case XK_asterisk: scancode = KEY_8; break;
			case XK_parenleft: scancode = KEY_9; break;
			case XK_parenright: scancode = KEY_0; break;
			case XK_minus: scancode = KEY_MINUS; break;
			case XK_underscore: scancode = KEY_MINUS; break;
			case XK_equal: scancode = KEY_EQUAL; break;
			case XK_plus: scancode = KEY_EQUAL; break;
			case XK_BackSpace: scancode = KEY_BACKSPACE; break;
			case XK_Tab: scancode = KEY_TAB; break;
			case XK_braceleft: scancode = KEY_LEFTBRACE; break;
			case XK_braceright: scancode = KEY_RIGHTBRACE; break;
			case XK_bracketleft: scancode = KEY_LEFTBRACE; break;
			case XK_bracketright: scancode = KEY_RIGHTBRACE; break;
			case XK_Return: scancode = KEY_ENTER; break;
			case XK_semicolon: scancode = KEY_SEMICOLON; break;
			case XK_colon: scancode = KEY_SEMICOLON; break;
			case XK_apostrophe: scancode = KEY_APOSTROPHE; break;
			case XK_quotedbl: scancode = KEY_APOSTROPHE; break;
			case XK_grave: scancode = KEY_GRAVE; break;
			case XK_asciitilde: scancode = KEY_GRAVE; break;
			case XK_backslash: scancode = KEY_BACKSLASH; break;
			case XK_bar: scancode = KEY_BACKSLASH; break;
			case XK_comma: scancode = KEY_COMMA; break;
			case XK_less: scancode = KEY_COMMA; break;
			case XK_period: scancode = KEY_DOT; break;
			case XK_greater: scancode = KEY_DOT; break;
			case XK_slash: scancode = KEY_SLASH; break;
			case XK_question: scancode = KEY_SLASH; break;
			case XK_Caps_Lock: scancode = KEY_CAPSLOCK; break;
			case XK_F1: scancode = KEY_F1; break;
			case XK_F2: scancode = KEY_F2; break;
			case XK_F3: scancode = KEY_F3; break;
			case XK_F4: scancode = KEY_F4; break;
			case XK_F5: scancode = KEY_F5; break;
			case XK_F6: scancode = KEY_F6; break;
			case XK_F7: scancode = KEY_F7; break;
			case XK_F8: scancode = KEY_F8; break;
			case XK_F9: scancode = KEY_F9; break;
			case XK_F10: scancode = KEY_F10; break;
			case XK_Num_Lock: scancode = KEY_NUMLOCK; break;
			case XK_Scroll_Lock: scancode = KEY_SCROLLLOCK; break;
			case XK_Page_Down: scancode = KEY_PAGEDOWN; break;
			case XK_Insert: scancode = KEY_INSERT; break;
			case XK_Delete: scancode = KEY_DELETE; break;
			case XK_Page_Up: scancode = KEY_PAGEUP; break;
			case XK_Escape: scancode = KEY_ESC; break;
			case XK_KP_Divide: scancode = KEY_KPSLASH; break;
			case XK_KP_Multiply: scancode = KEY_KPASTERISK; break;
			case XK_KP_Add: scancode = KEY_KPPLUS; break;
			case XK_KP_Subtract: scancode = KEY_KPMINUS; break;
			case XK_KP_Enter: scancode = KEY_KPENTER; break;
			case XK_KP_Decimal: scancode = KEY_KPDOT; break;
			case XK_KP_0: scancode = KEY_KP0; break;
			case XK_KP_1: scancode = KEY_KP1; break;
			case XK_KP_2: scancode = KEY_KP2; break;
			case XK_KP_3: scancode = KEY_KP3; break;
			case XK_KP_4: scancode = KEY_KP4; break;
			case XK_KP_5: scancode = KEY_KP5; break;
			case XK_KP_6: scancode = KEY_KP6; break;
			case XK_KP_7: scancode = KEY_KP7; break;
			case XK_KP_8: scancode = KEY_KP8; break;
			case XK_KP_9: scancode = KEY_KP9; break;
			case 0x0003: scancode = KEY_CENTER; break;
		}
	}
	return scancode;
}

void dokey(rfbBool down, rfbKeySym key, rfbClientPtr cl) {
	int scancode = keysym2scancode(key);
	bool was_down = down_keys[scancode];

	// Key press event
	if(down) {
		writeEvent(ukbd, EV_KEY, scancode, was_down ? 2 : 1); // Key repeat/press
		writeEvent(ukbd, EV_SYN, SYN_REPORT, 0); // Synchronization
		down_keys[scancode] = true;

	// Key release event
	} else {
		writeEvent(ukbd, EV_KEY, scancode, 0); // Key release
		writeEvent(ukbd, EV_SYN, SYN_REPORT, 0); // Synchronization
		down_keys[scancode] = false;
	}
}

void doptr(int buttonMask, int x, int y, rfbClientPtr cl) {
	//printf("DEBUG -> Mouse button mask: 0x%x, remote cursor position: X=%d, Y=%d.\n", buttonMask, x,y);

	// Mouse buttons and scroll events
	if (mouse_button != buttonMask) {

		// Left button
		if ((mouse_button & BTN_LEFT_MASK) != (buttonMask & BTN_LEFT_MASK)) {
			writeEvent(uptr, EV_KEY, BTN_LEFT, buttonMask & BTN_LEFT_MASK);
		}

		// Middle button
		if ((mouse_button & BTN_MIDDLE_MASK) != (buttonMask & BTN_MIDDLE_MASK)) {
			writeEvent(uptr, EV_KEY, BTN_MIDDLE, buttonMask & BTN_MIDDLE_MASK);
		}

		// Right button
		if ((mouse_button & BTN_RIGHT_MASK) != (buttonMask & BTN_RIGHT_MASK)) {
			writeEvent(uptr, EV_KEY, BTN_RIGHT, buttonMask & BTN_RIGHT_MASK);
		}

		// Wheel up
		if ((mouse_button & WHEEL_UP_MASK) != (buttonMask & WHEEL_UP_MASK)) {
			writeEvent(uptr, EV_REL, REL_WHEEL, BTN_LEFT_MASK);
		}

		// Wheel down
		if ((mouse_button & WHEEL_DOWN_MASK) != (buttonMask & WHEEL_DOWN_MASK)) {
			writeEvent(uptr, EV_REL, REL_WHEEL, -BTN_RIGHT_MASK);
		}

		// Set the current state as the last button state
		mouse_button = buttonMask;

		// Mouse movements -> To minimize CPU load only update the cursor on server side when mouse interaction occurs.
		if (mouse_x != x || mouse_y != y) {
			writeEvent(uptr, EV_ABS, ABS_X, x); // X-axis
			writeEvent(uptr, EV_ABS, ABS_Y, y); // Y-axis

			// Set the current position as the last position
			mouse_x = x;
			mouse_y = y;
		}

		// Synchronization
		writeEvent(uptr, EV_SYN, SYN_REPORT, 0);
	}
}
