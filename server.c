// SPDX-License-Identifier: LGPL-3.0-or-later
// VNC server core

#include "common.h"
#include "version.h"
#include "framebuffer.h"
#include "input.h"
#include "updatescreen.h"

int idle = 1;
int standby = 0;
int update_loop = 1;

// Connection variables
char serverHostname[256] = "";
char serverPassword[256] = "";
int serverPort = 5900;
char *reverseTarget = NULL;
int reversePort = 5500;

// Maximum FPS
int target_fps = 20;

ClientGoneHookPtr clientGone(rfbClientPtr cl) {
	return 0;
}

rfbNewClientHookPtr clientHook(rfbClientPtr cl) {
	cl->clientGoneHook=(ClientGoneHookPtr)clientGone;
	return RFB_CLIENT_ACCEPT;
}

void initReverseConnection(char *target) {
	char *separator;
	char host[256];
	rfbClientPtr cl;

	separator = strchr(target, ':');
	if (separator) {
		size_t len = separator - target;
		if (len >= sizeof(host))
			len = sizeof(host) - 1;
		memcpy(host, target, len);
		host[len] = '\0';
		reversePort = atoi(separator + 1);
		if (reversePort <= 0 || reversePort > 65535) {
			LOG("Invalid reverse port '%s'.\n", separator + 1);
			exit(EXIT_FAILURE);
		}
	} else {
		snprintf(host, sizeof(host), "%s", target);
	}

	cl = rfbReverseConnection(vncScreen, host, reversePort);
	if (!cl) {
		LOG("Failed to connect to reverse host: %s:%d.\n", host, reversePort);
		exit(EXIT_FAILURE);
	}

	cl->onHold = FALSE;
	rfbStartOnHoldClient(cl);
}

void initServer(int argc, char **argv) {
	if (serverPort <= 0 || serverPort > 65535) {
		LOG("Invalid server port: '%d'.\n", serverPort);
		exit(EXIT_FAILURE);
	}

	LOG("-- Initializing VNC server --\n");
	LOG(" Screen resolution: %dx%d, bit depth: %d bpp.\n",
		(int)screenFormat.width, (int)screenFormat.height, (int)screenFormat.bitsPerPixel);
	LOG(" RGBA colormap: %d:%d:%d, length: %d:%d:%d.\n",
		screenFormat.redShift, screenFormat.greenShift, screenFormat.blueShift,
		screenFormat.redMax, screenFormat.greenMax, screenFormat.blueMax);
	LOG(" Screen buffer size: %d bytes.\n", (int)(screenFormat.size));

	vncBuffer = calloc(screenFormat.width * screenFormat.height, screenFormat.bitsPerPixel / CHAR_BIT);
	assert(vncBuffer != NULL);

	vncScreen = rfbGetScreen(&argc, argv, screenFormat.width, screenFormat.height, 8, 3,  screenFormat.bitsPerPixel / CHAR_BIT);
	assert(vncScreen != NULL);

	vncScreen->desktopName = serverHostname;
	vncScreen->frameBuffer = (char *)vncBuffer;
	vncScreen->port = serverPort;
	vncScreen->ipv6port = serverPort;
	vncScreen->kbdAddEvent = addKeyboardEvent;
	vncScreen->ptrAddEvent = addPointerEvent;
	vncScreen->newClientHook = (rfbNewClientHookPtr)clientHook;

	if (strcmp(serverPassword, "") != 0) {
		char **passwords = malloc(2 * sizeof(char *));
		passwords[0] = serverPassword;
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

	LOG("-- Starting the server --\n");
	rfbInitServer(vncScreen);

	if (reverseTarget)
		initReverseConnection(reverseTarget);

	updateScreen(screenFormat.width, screenFormat.height, screenFormat.bitsPerPixel);
}

void sigHandler(int sig) {
	update_loop = 0;
}

void printUsage(char *str) {
	LOG("\nUsage: %s [options]\n"
		"-h               - Print this help\n"
		"-P <port>        - Listening port\n"
		"-n <name>        - Server name\n"
		"-p <password>    - Password to access server\n"
		"-R <host[:port]> - Host for reverse connection (default port: 5500)\n", str);
}

int main(int argc, char **argv) {
	struct timespec ts_now;
	uint64_t usec, time_limit, time_last, time_now;

	// Set the default server name based on the hostname
	gethostname(serverHostname, sizeof(serverHostname));

	// Preset values from environment variables (However, the values specified in the arguments have priority.)
	if (getenv("VNC_SERVERNAME"))
		snprintf(serverHostname, sizeof(serverHostname), "%s", getenv("VNC_SERVERNAME"));
	if (getenv("VNC_PASSWORD"))
		snprintf(serverPassword, sizeof(serverPassword), "%s", getenv("VNC_PASSWORD"));
	if (getenv("VNC_PORT"))
		serverPort = atoi(getenv("VNC_PORT"));

	LOG("AML-VNC Server v%d.%d.%d", MAIN_VERSION_MAJOR, MAIN_VERSION_MINOR, MAIN_VERSION_PATCH);
	if (MAIN_VERSION_BETA != 0)
		LOG(" Beta %d", MAIN_VERSION_BETA);
	LOG(" (Release date: %s)\n", MAIN_VERSION_DATE);

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
						if (++i >= argc || argv[i][0] == '-') {
							LOG("Missing argument for '%s'.\n", argv[i-1]);
							printUsage(argv[0]);
							exit(EXIT_FAILURE);
						}
						snprintf(serverHostname, sizeof(serverHostname), "%s", argv[i]);
						break;
					case 'p':
						if (++i >= argc || argv[i][0] == '-') {
							LOG("Missing argument for '%s'.\n", argv[i-1]);
							printUsage(argv[0]);
							exit(EXIT_FAILURE);
						}
						snprintf(serverPassword, sizeof(serverPassword), "%s", argv[i]);
						break;
					case 'P':
						if (++i >= argc || argv[i][0] == '-') {
							LOG("Missing argument for '%s'.\n", argv[i-1]);
							printUsage(argv[0]);
							exit(EXIT_FAILURE);
						}
						serverPort = atoi(argv[i]);
						break;
					case 'R':
						if (++i >= argc || argv[i][0] == '-') {
							LOG("Missing argument for '%s'.\n", argv[i-1]);
							printUsage(argv[0]);
							exit(EXIT_FAILURE);
						}
						reverseTarget = argv[i];
						break;
					default:
						LOG("Unknown option: %s\n", argv[i]);
						printUsage(argv[0]);
						exit(EXIT_FAILURE);
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

		if (!checkBufferStateChange()) {
			if (vncScreen->clientHead != NULL) {
				// Ignore events if they arrive before the next FPS expected
				clock_gettime(CLOCK_MONOTONIC, &ts_now);
				time_now = (uint64_t)ts_now.tv_sec * 1000000ULL + ts_now.tv_nsec / 1000ULL;
				if (time_now - time_last >= time_limit) {
					idle = updateScreen(screenFormat.width, screenFormat.height, screenFormat.bitsPerPixel);
					time_last = time_now;
				}
			}
		} else {
			LOG(" Reinitialization started...\n");
			rfbShutdownServer(vncScreen, TRUE);
			free(vncBuffer);
			rfbScreenCleanup(vncScreen);
			closeVirtualPointer();
			closeFrameBuffer();

			// Add a 1-second delay before reinit
			sleep(1);

			initFrameBuffer();
			initVirtualPointer();
			initServer(argc, argv);
		}
	}

	// VNC server shutdown
	LOG("-- Shutting down the server --\n");
	rfbShutdownServer(vncScreen, TRUE);
	free(vncScreen->frameBuffer);
	rfbScreenCleanup(vncScreen);

	LOG("-- Cleaning up --\n");
	closeFrameBuffer();
	closeVirtualKeyboard();
	closeVirtualPointer();

	return 0;
}
