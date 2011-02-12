# Makefile for FUNterm
# Cross-compiles on Linux for Win32 using MinGW compiler.


CC=mingw32-gcc
CCR=mingw32-windres
CFLAGS=-I.
DEPS = funtermres.h funterm.h serial.h
TARGET = FUNterm.exe
DOXYGEN = doxygen
SOURCES = funterm.c serial.c
OBJECTS = funterm.o serial.o funterm.res.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

%.res.o: %.rc $(DEPS)
	$(CCR) -i $< -o $@

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^  /usr/mingw32/usr/lib/libcomctl32.a -mwindows

clean:
	rm -f *.o $(TARGET)

docs/index.html: $(SOURCES) $(DEPS)
	$(DOXYGEN)

docs:  docs/index.html

