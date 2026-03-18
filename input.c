// SPDX-License-Identifier: LGPL-3.0-or-later
// Virtual input device handling

#include "input.h"

int virtKbd, virtPtr;
int downKeys[KEY_CNT];
int mouseX, mouseY;
int mouseButton = 0;

void initVirtualKeyboard(void) {
	struct uinput_user_dev uinpDev;
	int retcode, i;

	memset(downKeys, 0, sizeof(downKeys));

	LOG("-- Initializing virtual keyboard device --\n");

	virtKbd = open("/dev/uinput", O_WRONLY | O_NDELAY );
	if (virtKbd == 0) {
		LOG(" Could not open '/dev/uinput'.\n");
		exit(-1);
	}

	memset(&uinpDev, 0, sizeof(uinpDev));
	snprintf(uinpDev.name, UINPUT_MAX_NAME_SIZE, "VNC keysym device");
	uinpDev.id.version = 4;
	uinpDev.id.bustype = BUS_USB;

	ioctl(virtKbd, UI_SET_EVBIT, EV_SYN);
	ioctl(virtKbd, UI_SET_EVBIT, EV_KEY);

	for (i=0; i<KEY_MAX; i++) {
		ioctl(virtKbd, UI_SET_KEYBIT, i);
	}

	write(virtKbd, &uinpDev, sizeof(uinpDev));

	retcode = (ioctl(virtKbd, UI_DEV_CREATE));
	if (retcode) {
		LOG(" Error create virtual keyboard device.\n");
		exit(-1);
	} else {
		LOG(" The virtual keyboard device has been created.\n");
	}
}

void initVirtualPointer(void) {
	struct uinput_user_dev uinpDev;
	int retcode;

	LOG("-- Initializing virtual pointer device --\n");

	virtPtr = open("/dev/uinput", O_WRONLY | O_NDELAY );
	if (virtPtr == 0) {
		LOG(" Could not open '/dev/uinput'.\n");
		exit(-1);
	}

	memset(&uinpDev, 0, sizeof(uinpDev));
	snprintf(uinpDev.name, UINPUT_MAX_NAME_SIZE, "VNC virtual pointer");
	uinpDev.id.version = 1;
	uinpDev.id.bustype = BUS_USB;

	ioctl(virtPtr, UI_SET_EVBIT, EV_SYN);
	ioctl(virtPtr, UI_SET_EVBIT, EV_KEY);
	ioctl(virtPtr, UI_SET_EVBIT, EV_REL);
	ioctl(virtPtr, UI_SET_EVBIT, EV_ABS);

	ioctl(virtPtr, UI_SET_KEYBIT, BTN_LEFT);
	ioctl(virtPtr, UI_SET_KEYBIT, BTN_RIGHT);
	ioctl(virtPtr, UI_SET_KEYBIT, BTN_MIDDLE);

	ioctl(virtPtr, UI_SET_RELBIT, REL_WHEEL);

	ioctl(virtPtr, UI_SET_ABSBIT, ABS_X);
	ioctl(virtPtr, UI_SET_ABSBIT, ABS_Y);

	uinpDev.absmin[ABS_X] = 0;
	uinpDev.absmax[ABS_X] = screenFormat.width - 1;
	uinpDev.absmin[ABS_Y] = 0;
	uinpDev.absmax[ABS_Y] = screenFormat.height - 1;

	write(virtPtr, &uinpDev, sizeof(uinpDev));

	retcode = (ioctl(virtPtr, UI_DEV_CREATE));
	if (retcode) {
		LOG(" Error create virtual pointer device.\n");
		exit(-1);
	} else {
		LOG(" The virtual pointer device has been created.\n");
	}
}

void closeVirtualKeyboard(void) {
	ioctl(virtKbd, UI_DEV_DESTROY);
	close(virtKbd);
	LOG(" The virtual keyboard device has been deleted.\n");
}

void closeVirtualPointer(void) {
	ioctl(virtPtr, UI_DEV_DESTROY);
	close(virtPtr);
	LOG(" The virtual pointer device has been deleted.\n");
}

void writeEvent(int udev, uint16_t type, uint16_t code, int value) {
	struct input_event event;
	memset(&event, 0, sizeof(event));
	gettimeofday(&event.time, NULL);
	event.type = type;
	event.code = code;
	event.value = value;
	write(udev, &event, sizeof(event));
}

int keySym2Scancode(rfbKeySym key) {
	// LOG(" DEBUG -> Keyboard keysym key: %04X.\n", key);

	switch (key) {

	/* Modifiers */
	case XK_Shift_L:	return KEY_LEFTSHIFT;
	case XK_Shift_R:	return KEY_RIGHTSHIFT;
	case XK_Control_L:	return KEY_LEFTCTRL;
	case XK_Control_R:	return KEY_RIGHTCTRL;
	case XK_Alt_L:		return KEY_LEFTALT;
	case XK_Alt_R:
	case XK_ISO_Level3_Shift:
				return KEY_RIGHTALT;

	/* Alphabetic keys */
	case XK_a: case XK_A:	return KEY_A;
	case XK_b: case XK_B:	return KEY_B;
	case XK_c: case XK_C:	return KEY_C;
	case XK_d: case XK_D:	return KEY_D;
	case XK_e: case XK_E:	return KEY_E;
	case XK_f: case XK_F:	return KEY_F;
	case XK_g: case XK_G:	return KEY_G;
	case XK_h: case XK_H:	return KEY_H;
	case XK_i: case XK_I:	return KEY_I;
	case XK_j: case XK_J:	return KEY_J;
	case XK_k: case XK_K:	return KEY_K;
	case XK_l: case XK_L:	return KEY_L;
	case XK_m: case XK_M:	return KEY_M;
	case XK_n: case XK_N:	return KEY_N;
	case XK_o: case XK_O:	return KEY_O;
	case XK_p: case XK_P:	return KEY_P;
	case XK_q: case XK_Q:	return KEY_Q;
	case XK_r: case XK_R:	return KEY_R;
	case XK_s: case XK_S:	return KEY_S;
	case XK_t: case XK_T:	return KEY_T;
	case XK_u: case XK_U:	return KEY_U;
	case XK_v: case XK_V:	return KEY_V;
	case XK_w: case XK_W:	return KEY_W;
	case XK_x: case XK_X:	return KEY_X;
	case XK_y: case XK_Y:	return KEY_Y;
	case XK_z: case XK_Z:	return KEY_Z;

	/* Numeric keys */
	case XK_1:		return KEY_1;
	case XK_2:		return KEY_2;
	case XK_3:		return KEY_3;
	case XK_4:		return KEY_4;
	case XK_5:		return KEY_5;
	case XK_6:		return KEY_6;
	case XK_7:		return KEY_7;
	case XK_8:		return KEY_8;
	case XK_9:		return KEY_9;
	case XK_0:		return KEY_0;

	/* System and navigation keys */
	case XK_Escape:		return KEY_ESC;
	case XK_BackSpace:	return KEY_BACKSPACE;
	case XK_Tab:		return KEY_TAB;
	case XK_Return:		return KEY_ENTER;
	case XK_Insert:		return KEY_INSERT;
	case XK_Delete:		return KEY_DELETE;
	case XK_Home:		return KEY_HOME;
	case XK_Left:		return KEY_LEFT;
	case XK_Up:		return KEY_UP;
	case XK_Right:		return KEY_RIGHT;
	case XK_Down:		return KEY_DOWN;
	case XK_Page_Up:	return KEY_PAGEUP;
	case XK_Page_Down:	return KEY_PAGEDOWN;
	case XK_End:		return KEY_END;

	/* Function keys (F1-F12) */
	case XK_F1:		return KEY_F1;
	case XK_F2:		return KEY_F2;
	case XK_F3:		return KEY_F3;
	case XK_F4:		return KEY_F4;
	case XK_F5:		return KEY_F5;
	case XK_F6:		return KEY_F6;
	case XK_F7:		return KEY_F7;
	case XK_F8:		return KEY_F8;
	case XK_F9:		return KEY_F9;
	case XK_F10:		return KEY_F10;
	case XK_F11:		return KEY_F11;
	case XK_F12:		return KEY_F12;

	/* Physical punctuation keys */
	case XK_space:		return KEY_SPACE;
	case XK_minus:		return KEY_MINUS;
	case XK_equal:		return KEY_EQUAL;
	case XK_bracketleft:	return KEY_LEFTBRACE;
	case XK_bracketright:	return KEY_RIGHTBRACE;
	case XK_semicolon:	return KEY_SEMICOLON;
	case XK_apostrophe:	return KEY_APOSTROPHE;
	case XK_grave:		return KEY_GRAVE;
	case XK_backslash:	return KEY_BACKSLASH;
	case XK_comma:		return KEY_COMMA;
	case XK_period:		return KEY_DOT;
	case XK_slash:		return KEY_SLASH;

	/* Layout dependent keys - Used together with modifiers (US) */
	case XK_exclam:		return KEY_1;
	case XK_at:		return KEY_2;
	case XK_numbersign:	return KEY_3;
	case XK_dollar:		return KEY_4;
	case XK_percent:	return KEY_5;
	case XK_asciicircum:	return KEY_6;
	case XK_ampersand:	return KEY_7;
	case XK_parenleft:	return KEY_9;
	case XK_parenright:	return KEY_0;
	case XK_underscore:	return KEY_MINUS;
	case XK_colon:		return KEY_SEMICOLON;
	case XK_quotedbl:	return KEY_APOSTROPHE;
	case XK_asciitilde:	return KEY_GRAVE;
	case XK_bar:		return KEY_BACKSLASH;
	case XK_less:		return KEY_COMMA;
	case XK_greater:	return KEY_DOT;
	case XK_question:	return KEY_SLASH;

	/* Numeric keypad - Independent of server-side Num Lock state */
	case XK_KP_Divide:	return KEY_KPSLASH;
	case XK_KP_Multiply:	return KEY_KPASTERISK;
	case XK_KP_Add:		return KEY_KPPLUS;
	case XK_KP_Subtract:	return KEY_KPMINUS;
	case XK_KP_Enter:	return KEY_KPENTER;
	case XK_KP_Decimal:	return KEY_KPDOT;
	case XK_KP_0:		return KEY_0;
	case XK_KP_1:		return KEY_1;
	case XK_KP_2:		return KEY_2;
	case XK_KP_3:		return KEY_3;
	case XK_KP_4:		return KEY_4;
	case XK_KP_5:		return KEY_5;
	case XK_KP_6:		return KEY_6;
	case XK_KP_7:		return KEY_7;
	case XK_KP_8:		return KEY_8;
	case XK_KP_9:		return KEY_9;
	case XK_KP_Home:	return KEY_HOME;
	case XK_KP_End:		return KEY_END;
	case XK_KP_Page_Up:	return KEY_PAGEUP;
	case XK_KP_Page_Down:	return KEY_PAGEDOWN;
	case XK_KP_Up:		return KEY_UP;
	case XK_KP_Down:	return KEY_DOWN;
	case XK_KP_Left:	return KEY_LEFT;
	case XK_KP_Right:	return KEY_RIGHT;
	case XK_KP_Insert:	return KEY_INSERT;
	case XK_KP_Delete:	return KEY_DELETE;

	/* Redefined keys */
	case XK_asterisk:	return KEY_KPASTERISK;
	case XK_plus:		return KEY_KPPLUS;

	/* Unhandled keys */
	default:		return 0;
	}
}

void addKeyboardEvent(rfbBool down, rfbKeySym key, rfbClientPtr cl) {
	int scancode = keySym2Scancode(key);
	int wasDown = downKeys[scancode];

	// Reset synchronization request
	int needSync = 0;

	// Key repeat and press event
	if(down) {
		writeEvent(virtKbd, EV_KEY, scancode, wasDown ? 2 : 1); // Auto-repeat (2), Press (1)
		downKeys[scancode] = 1;
		needSync = 1;

	// Key release event
	} else {
		writeEvent(virtKbd, EV_KEY, scancode, 0); // Release (0)
		downKeys[scancode] = 0;
		needSync = 1;
	}

	// Synchronization
	if (needSync) {
		writeEvent(virtKbd, EV_SYN, SYN_REPORT, 0);
	}
}

void addPointerEvent(int buttonMask, int x, int y, rfbClientPtr cl) {
	// LOG(" DEBUG -> Last button mask: 0x%x, current button mask: 0x%x, cursor position: X=%d, Y=%d.\n", mouseButton, buttonMask, x, y);

	// Reset synchronization request
	int needSync = 0;

	// Mouse cursor events are only processed when movement occurs
	if (mouseX != x || mouseY != y) {

		// Set cursor coordinates
		writeEvent(virtPtr, EV_ABS, ABS_X, x); // X axis
		writeEvent(virtPtr, EV_ABS, ABS_Y, y); // Y axis
		needSync = 1;

		// Set the current position as the last position
		mouseX = x;
		mouseY = y;
	}

	// Button and scrool wheel events are only processed when a state change occurs
	if (mouseButton != buttonMask) {

		// Left button
		if ((mouseButton & BTN_LEFT_MASK) != (buttonMask & BTN_LEFT_MASK)) {
			writeEvent(virtPtr, EV_KEY, BTN_LEFT, (buttonMask & BTN_LEFT_MASK) ? 1 : 0);
			needSync = 1;
		}

		// Middle button
		if ((mouseButton & BTN_MIDDLE_MASK) != (buttonMask & BTN_MIDDLE_MASK)) {
			writeEvent(virtPtr, EV_KEY, BTN_MIDDLE, (buttonMask & BTN_MIDDLE_MASK) ? 1 : 0);
			needSync = 1;
		}

		// Right button
		if ((mouseButton & BTN_RIGHT_MASK) != (buttonMask & BTN_RIGHT_MASK)) {
			writeEvent(virtPtr, EV_KEY, BTN_RIGHT, (buttonMask & BTN_RIGHT_MASK) ? 1 : 0);
			needSync = 1;
		}

		// Scroll wheel up
		if (!(mouseButton & WHEEL_UP_MASK) && (buttonMask & WHEEL_UP_MASK)) {
			writeEvent(virtPtr, EV_REL, REL_WHEEL, 1);
			needSync = 1;
		}

		// Scroll wheel down
		if (!(mouseButton & WHEEL_DOWN_MASK) && (buttonMask & WHEEL_DOWN_MASK)) {
			writeEvent(virtPtr, EV_REL, REL_WHEEL, -1);
			needSync = 1;
		}

		// Set the current state as the last button state
		mouseButton = buttonMask;
	}

	// Synchronization
	if (needSync) {
		writeEvent(virtPtr, EV_SYN, SYN_REPORT, 0);
	}
}
