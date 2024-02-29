#ifndef IMAGE_SCALING_H
#define IMAGE_SCALING_H

#include "png_image.h"

/*
 * scales an image to a new width and height using nearest neighbor scaling
 * returns a newly created PNG_Image struct
 * does not deallocate the original PNG_Image at all
 */
PNG_Image* nearest_neighbor_scale(PNG_Image* orig, int new_width, int new_height);

/*
 * scales an image to a new width and height using nearest bilinear interpolation scaling
 * returns a newly created PNG_Image struct
 * does not deallocate the original PNG_Image at all
 */
PNG_Image* bilinear_interpolation_scale(PNG_Image* orig, int new_width, int new_height);

#endif // IMAGE_SCALING_H
