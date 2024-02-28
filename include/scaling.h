#ifndef IMAGE_SCALING_H
#define IMAGE_SCALING_H

#include "png_image.h"

// Function to scale an image using nearest neighbor algorithm
PNG_Image* nearest_neighbor_scale(PNG_Image* orig, int new_width, int new_height);

// Function to scale an image using bilinear interpolation algorithm
PNG_Image* bilinear_interpolation_scale(PNG_Image* orig, int new_width, int new_height);

#endif // IMAGE_SCALING_H
