#ifndef X11_DISPLAY_H
#define X11_DISPLAY_H

#include <X11/Xlib.h>
#include <stdlib.h>

// Forward declarations of types used in the functions
typedef unsigned char png_byte;

// Function to load a PNG image from a file and create an XImage
XImage* load_png_from_file(Display* display, char* filepath);

// Function to load a PNG image from memory and create an XImage
XImage* load_png_from_memory(Display* display, png_byte* memory, size_t memory_size);

#endif // X11_DISPLAY_H
