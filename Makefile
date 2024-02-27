CC=gcc
CFLAGS=-I./include -I/usr/include/X11 -L/usr/lib/X11 -Wall -Wunused
LDFLAGS=-lX11 -lpng -lm
SRCDIR=src
BUILDDIR=build
LIB_TARGET=$(BUILDDIR)/libnagato.a  # Static library
TEST_TARGET=$(BUILDDIR)/test_executable  # Testing executable
LIB_OBJFILES=$(BUILDDIR)/png_loading.o $(BUILDDIR)/windowing.o $(BUILDDIR)/scaling.o  # Library object files
TEST_OBJFILES=$(BUILDDIR)/main.o  # Test executable object files

all: $(LIB_TARGET) $(TEST_TARGET)

$(LIB_TARGET): $(LIB_OBJFILES)
	mkdir -p $(BUILDDIR)
	ar rcs $@ $^

$(TEST_TARGET): $(TEST_OBJFILES) $(LIB_TARGET)
	$(CC) $(TEST_OBJFILES) -o $@ -L$(BUILDDIR) -lnagato $(CFLAGS) $(LDFLAGS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILDDIR)