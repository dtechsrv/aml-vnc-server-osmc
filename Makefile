CC ?= gcc

PKG_CFLAGS := $(shell pkg-config --cflags libdrm)
PKG_LIBS := $(shell pkg-config --libs libdrm)

CFLAGS += -Wall $(PKG_CFLAGS)
LDLIBS += -lvncserver -lpng -ljpeg -lpthread -lssl -lcrypto -lz -lresolv -lm $(PKG_LIBS)

OBJS := updatescreen.o framebuffer.o input.o server.o

TARGET := aml-vnc

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

clean:
	$(RM) -f $(OBJS) $(TARGET)
