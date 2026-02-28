CC ?= gcc
PKG_CONFIG ?= pkg-config

SOURCE_DIR := .
BACKEND_DIR := backend

CFLAGS += -Wall -I$(SOURCE_DIR) -I$(BACKEND_DIR)
LDFLAGS += -lvncserver -lpng -ljpeg -lpthread -lssl -lcrypto -lz -lresolv -lm

SOURCES := framebuffer.c updatescreen.c input.c server.c $(BACKEND_DIR)/fbdev.c

HAVE_LIBDRM := $(shell $(PKG_CONFIG) --exists libdrm 2>/dev/null && echo 1 || echo 0)

ifeq ($(HAVE_LIBDRM),1)
CFLAGS += $(shell $(PKG_CONFIG) --cflags libdrm) -DHAVE_LIBDRM
LDFLAGS += $(shell $(PKG_CONFIG) --libs libdrm)
SOURCES += $(BACKEND_DIR)/drm.c
endif

OBJS := $(SOURCES:.c=.o)

TARGET := aml-vnc

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	$(RM) -f $(OBJS) $(TARGET)
