#ifndef IMAGE_SCALING_H
#define IMAGE_SCALING_H

#include <X11/Xlib.h>

// Function to scale an image using nearest neighbor algorithm
XImage* nearest_neighbor_scale(Display* display, Window window, XImage* orig, int new_width, int new_height);

// Function to scale an image using bilinear interpolation algorithm
XImage* bilinear_interpolation_scale(Display* display, Window window, XImage* orig, int new_width, int new_height);

#endif // IMAGE_SCALING_H
