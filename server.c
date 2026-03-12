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
int clientSession = 0;

// Maximum FPS
int target_fps = 20;

// Options
int disablePointer = 0;
int printVncDebug = 0;

void clientDisconnect(rfbClientPtr cl) {
	if (!printVncDebug)
		LOG(" [%d] Client disconnected.\n", (int)(intptr_t)cl->clientData);
}

enum rfbNewClientAction clientConnect(rfbClientPtr cl) {
	cl->clientData = (void*)(intptr_t)(++clientSession);
	if (!printVncDebug)
		LOG(" [%d] Client connected from %s.\n", (int)(intptr_t)cl->clientData, cl->host);
	cl->clientGoneHook = clientDisconnect;
	return RFB_CLIENT_ACCEPT;
}

rfbBool checkPassword(rfbClientPtr cl, const char* response, int len) {
	if (rfbCheckPasswordByList(cl, response, len)) {
		if (!printVncDebug)
			LOG(" [%d] Client authentication successful.\n", (int)(intptr_t)cl->clientData);
		return TRUE;
	} else {
		if (!printVncDebug)
			LOG(" [%d] Client authentication failed.\n", (int)(intptr_t)cl->clientData);
		return FALSE;
	}
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

void initServer(void) {
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

	vncScreen = rfbGetScreen(NULL, NULL, screenFormat.width, screenFormat.height, 8, 3,  screenFormat.bitsPerPixel / CHAR_BIT);
	assert(vncScreen != NULL);

	vncScreen->desktopName = serverHostname;
	vncScreen->frameBuffer = (char *)vncBuffer;
	vncScreen->port = serverPort;
	vncScreen->ipv6port = serverPort;
	vncScreen->kbdAddEvent = addKeyboardEvent;

	if (!disablePointer)
		vncScreen->ptrAddEvent = addPointerEvent;

	vncScreen->newClientHook = clientConnect;

	if (strcmp(serverPassword, "") != 0) {
		char **passwords = malloc(2 * sizeof(char *));
		passwords[0] = serverPassword;
		passwords[1] = NULL;
		vncScreen->authPasswdData = passwords;
		vncScreen->passwordCheck = checkPassword;
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

	rfbLogEnable(printVncDebug);

	LOG("-- Starting the server --\n");
	rfbInitServer(vncScreen);

	if (reverseTarget)
		initReverseConnection(reverseTarget);

	if (!printVncDebug)
		LOG(" Debug output from libvncserver has been disabled.\n");

	updateScreen(screenFormat.width, screenFormat.height, screenFormat.bitsPerPixel);
}

void sigHandler(int sig) {
	update_loop = 0;
}

void printUsage(char *str) {
	LOG("\nUsage: %s [options]\n"
		"-h | -?          - Print this help\n"
		"-P <port>        - Listening port\n"
		"-n <name>        - Server name\n"
		"-p <password>    - Password to access server\n"
		"-R <host[:port]> - Host for reverse connection (default port: 5500)\n"
		"-m               - Mouseless mode (disable virtual pointer)\n"
		"-d               - Print libvncserver debug output\n", str);
}

void serverStateChange(int state) {
	if (state == SERVER_STOP || state == SERVER_REINIT) {
		rfbShutdownServer(vncScreen, TRUE);
		free(vncScreen->frameBuffer);
		rfbScreenCleanup(vncScreen);
		closeFrameBuffer();
		if (state == SERVER_STOP)
			closeVirtualKeyboard();
		if (!disablePointer)
			closeVirtualPointer();
	}

	if (state == SERVER_REINIT && reinitDelay > 0)
		usleep(reinitDelay * 1000);

	if (state == SERVER_INIT || state == SERVER_REINIT) {
		initFrameBuffer();
		if (state == SERVER_INIT)
			initVirtualKeyboard();
		if (!disablePointer)
			initVirtualPointer();
		initServer();
	}
}

int main(int argc, char **argv) {
	struct timespec ts_now;
	uint64_t usec, time_limit, time_last, time_now;
	int i;

	// Set the default server name based on the hostname
	gethostname(serverHostname, sizeof(serverHostname));

	// Preset values from environment variables (However, the values specified in the arguments have priority.)
	if (getenv("VNC_SERVERNAME"))
		snprintf(serverHostname, sizeof(serverHostname), "%s", getenv("VNC_SERVERNAME"));
	if (getenv("VNC_PASSWORD"))
		snprintf(serverPassword, sizeof(serverPassword), "%s", getenv("VNC_PASSWORD"));
	if (getenv("VNC_PORT"))
		serverPort = atoi(getenv("VNC_PORT"));
	if (getenv("VNC_NOMOUSE") && !strcasecmp(getenv("VNC_NOMOUSE"), "true"))
		disablePointer = 1;
	if (getenv("VNC_DEBUGLOG") && !strcasecmp(getenv("VNC_DEBUGLOG"), "true"))
		printVncDebug = 1;

	LOG("AML-VNC Server v%d.%d.%d", MAIN_VERSION_MAJOR, MAIN_VERSION_MINOR, MAIN_VERSION_PATCH);
	if (MAIN_VERSION_BETA != 0)
		LOG(" Beta %d", MAIN_VERSION_BETA);
	LOG(" (Release date: %s)\n", MAIN_VERSION_DATE);

	for (i = 1; i < argc; i++) {
		if (*argv[i] == '-') {
			switch (*(argv[i] + 1)) {
			case 'h':
			case '?':
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
			case 'm':
				disablePointer = 1;
				break;
			case 'd':
				printVncDebug = 1;
				break;
			default:
				LOG("Unknown option: %s\n", argv[i]);
				printUsage(argv[0]);
				exit(EXIT_FAILURE);
			}
		}
	}

	// Start initialization
	srand(time(NULL));
	serverStateChange(SERVER_INIT);
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
			LOG("-- Server reinitialization started --\n");
			serverStateChange(SERVER_REINIT);
		}
	}

	LOG("-- Shutting down the server --\n");
	serverStateChange(SERVER_STOP);

	return 0;
}
