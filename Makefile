CC=gcc
CFLAGS=-I/usr/include/X11 -L/usr/lib/X11 -lX11 -lpng
SRCDIR=src
BUILDDIR=build
TARGET=$(BUILDDIR)/main

all: $(TARGET)

$(TARGET): $(SRCDIR)/main.c
	mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf $(BUILDDIR)