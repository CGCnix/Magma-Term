.POSIX:
.SUFFIXES: .c .o

CC=cc
INCLUDES=-I ./includes -I /usr/include/freetype2
TARGET=magma
COBJS=src/main.o src/backend/backend.o src/backend/xcb.o
LIBS=-lxcb -lxcb-image -lfreetype

.c.o:
	$(CC) $(INCLUDES) -c -o $@ $<

$(TARGET): $(COBJS)
	$(CC) $(INCLUDES) $(LIBS) -o $@ $(COBJS)

clean:
	rm $(TARGET) $(COBJS)
