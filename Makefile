CC=gcc
CFLAGS=-I./include -I/usr/include/X11 -L/usr/lib/X11 -lX11 -lpng -Wall -Wunused
SRCDIR=src
BUILDDIR=build
TARGET=$(BUILDDIR)/main
OBJFILES=$(BUILDDIR)/main.o $(BUILDDIR)/png_loading.o

all: $(TARGET)

$(TARGET): $(OBJFILES)
	mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILDDIR)/main.o: $(SRCDIR)/main.c
	mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/png_loading.o: $(SRCDIR)/png_loading.c
	mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILDDIR)