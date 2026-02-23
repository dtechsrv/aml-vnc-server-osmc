/*
droid vnc server - Android VNC server
Copyright (C) 2009 Jose Pereira <onaips@gmail.com>

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

#include "common.h"
#include "version.h"
#include "framebuffer.h"
#include "input.h"
#include "updatescreen.h"

#include <time.h>
#include <signal.h>

int idle = 1;
int standby = 0;
int update_loop = 1;

char VNC_SERVERNAME[256];
char VNC_PASSWORD[256];
int VNC_PORT = 5900;

// Reverse connection
char *rhost = NULL;
int rport = 5500;

// Maximum FPS
int target_fps = 20;

ClientGoneHookPtr clientGone(rfbClientPtr cl) {
	return 0;
}

rfbNewClientHookPtr clientHook(rfbClientPtr cl) {
	cl->clientGoneHook=(ClientGoneHookPtr)clientGone;
	return RFB_CLIENT_ACCEPT;
}

void extractReverseHostPort(char *str) {
	int len = sizeof(str);
	char *p;
	/* copy in to host */
	rhost = (char *) malloc(len);
	if (! rhost) {
		L(" Reverse connection: could not malloc string %d.\n", len);
		exit(-1);
	}
	snprintf(rhost, len, str);

	/* Extract port, if any */
	if ((p = strrchr(rhost, ':')) != NULL) {
		rport = atoi(p + 1);
		if (rport < 0) {
			rport = -rport;
		}
		else if (rport < 20) {
			rport = 5500 + rport;
		}
		*p = '\0';
	}
}

void initReverseConnection(void) {
	rfbClientPtr cl;
	cl = rfbReverseConnection(vncScreen, rhost, rport);
	if (cl == NULL) {
		char *str = malloc(255 * sizeof(char));
		L(" Couldn't connect to remote host: %s.\n",rhost);
		free(str);
	} else {
		cl->onHold = FALSE;
		rfbStartOnHoldClient(cl);
	}
}

void initServer(int argc, char **argv) {
	L("-- Initializing VNC server --\n");
	L(" Screen resolution: %dx%d, bit depth: %d bpp.\n",
		(int)screenFormat.width, (int)screenFormat.height, (int)screenFormat.bitsPerPixel);
	L(" RGBA colormap: %d:%d:%d, length: %d:%d:%d.\n",
		screenFormat.redShift, screenFormat.greenShift, screenFormat.blueShift,
		screenFormat.redMax, screenFormat.greenMax, screenFormat.blueMax);
	L(" Screen buffer size: %d bytes.\n", (int)(screenFormat.size));

	vncBuffer = calloc(screenFormat.width * screenFormat.height, screenFormat.bitsPerPixel / CHAR_BIT);
	assert(vncBuffer != NULL);

	vncScreen = rfbGetScreen(&argc, argv, screenFormat.width, screenFormat.height, 0 /* not used */ , 3,  screenFormat.bitsPerPixel / CHAR_BIT);
	assert(vncScreen != NULL);

	vncScreen->desktopName = VNC_SERVERNAME;
	vncScreen->frameBuffer = (char *)vncBuffer;
	vncScreen->port = VNC_PORT;
	vncScreen->ipv6port = VNC_PORT;
	vncScreen->kbdAddEvent = addKeyboardEvent;
	vncScreen->ptrAddEvent = addPointerEvent;
	vncScreen->newClientHook = (rfbNewClientHookPtr)clientHook;

	if (strcmp(VNC_PASSWORD, "") != 0) {
		char **passwords = (char **)malloc(2 * sizeof(char **));
		passwords[0] = VNC_PASSWORD;
		passwords[1] = NULL;
		vncScreen->authPasswdData = passwords;
		vncScreen->passwordCheck = rfbCheckPasswordByList;
	}

	vncScreen->serverFormat.redShift = screenFormat.redShift;
	vncScreen->serverFormat.greenShift = screenFormat.greenShift;
	vncScreen->serverFormat.blueShift = screenFormat.blueShift;

	vncScreen->serverFormat.redMax = (( 1 << screenFormat.redMax) -1);
	vncScreen->serverFormat.greenMax = (( 1 << screenFormat.greenMax) -1);
	vncScreen->serverFormat.blueMax = (( 1 << screenFormat.blueMax) -1);

	vncScreen->serverFormat.trueColour = TRUE;
	vncScreen->serverFormat.bitsPerPixel = screenFormat.bitsPerPixel;

	vncScreen->alwaysShared = TRUE;

	L("-- Starting the server --\n");
	rfbInitServer(vncScreen);

	if (rhost)
		initReverseConnection();

	updateScreen(screenFormat.width, screenFormat.height, screenFormat.bitsPerPixel);
}

void sigHandler() {
	update_loop = 0;
}

void printUsage(char *str) {
	L("A framebuffer based VNC Server for Amlogic devices\n\n"
		"Usage: %s [parameters]\n"
		"-h\t\t- Print this help\n"
		"-P <port>\t- Listening port\n"
		"-n <name>\t- Server name\n"
		"-p <password>\t- Password to access server\n"
		"-R <host:port>\t- Host for reverse connection\n", str);
}

int main(int argc, char **argv) {
	struct timespec ts_now;
	uint64_t usec, time_limit, time_last, time_now;

	// Set the default server name based on the hostname
	gethostname(VNC_SERVERNAME, sizeof(VNC_SERVERNAME));

	// Preset values from environment variables (However, the values specified in the arguments have priority.)
	if (getenv("VNC_SERVERNAME"))
		snprintf(VNC_SERVERNAME, sizeof(VNC_SERVERNAME), getenv("VNC_SERVERNAME"));
	if (getenv("VNC_PASSWORD"))
		snprintf(VNC_PASSWORD, sizeof(VNC_PASSWORD), getenv("VNC_PASSWORD"));
	if (getenv("VNC_PORT"))
		VNC_PORT = atoi(getenv("VNC_PORT"));

	L("AML-VNC Server v%d.%d.%d", MAIN_VERSION_MAJOR, MAIN_VERSION_MINOR, MAIN_VERSION_PATCH);
	if (MAIN_VERSION_BETA != 0)
		L(" Beta %d", MAIN_VERSION_BETA);
	L(" (Release date: %s)\n", MAIN_VERSION_DATE);

	if(argc > 1) {
		int i = 1;
		while(i < argc) {
			if(*argv[i] == '-') {
				switch(*(argv[i] + 1)) {
					case 'h':
						printUsage(argv[0]);
						exit(0);
						break;
					case 'n':
						i++;
						snprintf(VNC_SERVERNAME, sizeof(VNC_SERVERNAME), argv[i]);
						break;
					case 'p':
						i++;
						snprintf(VNC_PASSWORD, sizeof(VNC_PASSWORD), argv[i]);
						break;
					case 'P':
						i++;
						VNC_PORT=atoi(argv[i]);
						break;
					case 'R':
						i++;
						extractReverseHostPort(argv[i]);
						break;
				}
			}
		i++;
		}
	}

	// Start initialization
	srand(time(NULL));
	initFrameBuffer();
	initVirtualKeyboard();
	initVirtualPointer();
	initServer(argc, argv);

	signal(SIGINT, sigHandler);

	// Set refresh cycle check values
	time_limit = 1000000ULL / target_fps;
	time_last = 0;

	// Start the update loop
	while (update_loop) {
		usec = (vncScreen->deferUpdateTime + standby) * 1000;
		rfbProcessEvents(vncScreen, usec);
		if (idle) {
			standby = 100;
		} else {
			standby = 10;
		}

		if (vncScreen->clientHead != NULL) {
			if (!checkResolutionChange()) {
				// Ignore events if they arrive before the next FPS expected
				clock_gettime(CLOCK_MONOTONIC, &ts_now);
				time_now = (uint64_t)ts_now.tv_sec * 1000000ULL + ts_now.tv_nsec / 1000ULL;
				if (time_now - time_last >= time_limit) {
					idle = updateScreen(screenFormat.width, screenFormat.height, screenFormat.bitsPerPixel);
					time_last = time_now;
				}
			} else {
				L(" Reinitialization started...\n");
				rfbShutdownServer(vncScreen, TRUE);
				free(vncScreen->frameBuffer);
				rfbScreenCleanup(vncScreen);
				closeVirtualPointer();
				closeFrameBuffer();
				initFrameBuffer();
				initVirtualPointer();
				initServer(argc, argv);
			}
		}
	}

	// VNC server shutdown
	L("-- Shutting down the server --\n");
	rfbShutdownServer(vncScreen, TRUE);
	free(vncScreen->frameBuffer);
	rfbScreenCleanup(vncScreen);

	L("-- Cleaning up --\n");
	closeFrameBuffer();
	closeVirtualKeyboard();
	closeVirtualPointer();

	return 0;
}
