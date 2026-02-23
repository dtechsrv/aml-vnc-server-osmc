OBJS := updatescreen.c framebuffer.c input.c server.c
LIBS := -lvncserver -lpng -ljpeg -lpthread -lssl -lcrypto -lz -lresolv -lm
CFLAGS += -Wall

all: aml-vnc

aml-vnc: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

clean:
	$(RM) -rf
