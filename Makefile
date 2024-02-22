CC=gcc
CFLAGS=-I./include -I/usr/include/X11 -L/usr/lib/X11 -lX11 -lpng -lm -Wall -Wunused
SRCDIR=src
BUILDDIR=build
TARGET=$(BUILDDIR)/main
OBJFILES=$(BUILDDIR)/main.o $(BUILDDIR)/png_loading.o $(BUILDDIR)/windowing.o $(BUILDDIR)/scaling.o

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

$(BUILDDIR)/windowing.o: $(SRCDIR)/windowing.c
	mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/scaling.o: $(SRCDIR)/scaling.c
	mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILDDIR)