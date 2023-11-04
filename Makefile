.POSIX:
.SUFFIXES: .c .o

CC=gcc
INCLUDES=-I ./includes -I /usr/include/libdrm -I /usr/include/freetype2
TARGET=magma
COBJS=src/main.o src/logger/log.o src/backend/wl.o src/backend/backend.o src/backend/xcb.o src/backend/drm.o

LIBS=-lxcb -lxcb-image -lfreetype -lxkbcommon -ldrm -lxkbcommon-x11

.c.o:
	$(CC) $(INCLUDES) -c -o $@ $<

$(TARGET): $(COBJS)
	$(CC) $(INCLUDES) $(LIBS) -o $@ $(COBJS)

clean:
	rm $(TARGET) $(COBJS)
