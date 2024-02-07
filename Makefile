CC=gcc
CFLAGS=-Iinclude
LDFLAGS=-Lbuild -lpthread
DEPS = include/my_gui.h include/renderer.h
SRC = src/my_gui.c src/renderer.c src/test.c
OBJ = $(SRC:src/%.c=build/%.o)
LIB_NAME=build/libmygui.a
TEST_NAME=build/test

# Default target
all: $(LIB_NAME) $(TEST_NAME)

# Create build directory
$(shell mkdir -p build)

# Compile .c to .o
build/%.o: src/%.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

# Create static library
$(LIB_NAME): build/my_gui.o build/renderer.o
	ar rcs $@ $^

# Compile test program
$(TEST_NAME): build/test.o $(LIB_NAME)
	$(CC) $< -o $@ $(LDFLAGS) -lmygui

# Clean up
clean:
	rm -rf ./build/*.o

.PHONY: all clean
