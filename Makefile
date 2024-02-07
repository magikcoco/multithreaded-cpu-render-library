CC=gcc
CFLAGS=-Iinclude -lX11
LDFLAGS=-Lbuild -lpthread -lmygui -lX11
DEPS = include/my_gui.h include/renderer.h include/window.h
LIB_SRC = src/my_gui.c src/renderer.c src/window.c
TEST_SRC = tests/test.c
LIB_OBJ = $(LIB_SRC:src/%.c=build/%.o)
TEST_OBJ = $(TEST_SRC:tests/%.c=build/%.o)
LIB_NAME=build/libmygui.a
TEST_NAME=build/test

# Default target
all: $(LIB_NAME) $(TEST_NAME)

# Create build directory
$(shell mkdir -p build)

# Compile library .c to .o
build/%.o: src/%.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile test .c to .o
build/%.o: tests/%.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

# Create static library
$(LIB_NAME): $(LIB_OBJ)
	ar rcs $@ $^

# Compile test program
$(TEST_NAME): $(TEST_OBJ) $(LIB_NAME)
	$(CC) $< -o $@ $(LDFLAGS)

# Clean up
clean:
	rm -rf ./build/*.o ./build/test ./build/libmygui.a

.PHONY: all clean