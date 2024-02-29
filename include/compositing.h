#ifndef COMPOSITING_H
#define COMPOSITING_H

#include <png.h>

typedef struct {
    png_byte red, green, blue, alpha;
} rgba_color;

//TODO: function to queue up images to be flattened into a single image
//TODO: function to flatten all queued iamges into a single image

/*
 * Blends an image with a specified background color by modifying the image's pixel data in place
 * Assumes pixels are represented as four consecutive bytes (RGBA: Red, Green, Blue, Alpha) in a flat array
 * Sets the opacity to full for every pixel
 */
void blend_with_background(png_bytep img_data, int width, int height, rgba_color background_color);

#endif