#ifndef COMPOSITING_H
#define COMPOSITING_H

#include <png.h>

typedef struct {
    png_byte red, green, blue, alpha;
} rgba_color;

void blend_with_background(png_bytep img_data, int width, int height, rgba_color background_color);

#endif