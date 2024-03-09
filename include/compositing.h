#ifndef COMPOSITING_H
#define COMPOSITING_H

#include <png.h>
#include <pthread.h>
#include "png_image.h"

typedef struct {
    png_byte red, green, blue, alpha;
} rgba_color;

//TODO: function to flatten all queued iamges into a single image

void destroy_image_stack();
void clear_image_stack();

// Image flattening
void push_image(PNG_Image* image, int x, int y);
PNG_Image* flatten_stack_into_image();

// Other image manipulation

/*
 * Blends an image with a specified background color by modifying the image's pixel data in place
 * Assumes pixels are represented as four consecutive bytes (RGBA: Red, Green, Blue, Alpha) in a flat array
 * Sets the opacity to full for every pixel
 */
void blend_with_background(png_bytep img_data, int width, int height, rgba_color background_color);

#endif