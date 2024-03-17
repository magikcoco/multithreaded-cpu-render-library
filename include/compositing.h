#ifndef COMPOSITING_H
#define COMPOSITING_H

#include <png.h>
#include <pthread.h>
#include "png_image.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////// IMAGE FLATTENING ///////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * Mark the given image to be rendered in a single flat image, obtained by calling get_flattened_image().
 * Images pushed sooner will overlap with and draw on top of images pushed later.
 * Images are rendered at the given X & Y coordinates, where the top left corner is (0, 0). X increases going right and Y increases going down.
 * The last image to be pushed is used as the background, and pixels that fall outside of it's boundaries will be discarded.
 * Images pushed in this way are not deallocated.
 */
void push_image_raw(const PNG_Image *const image, int x, int y);

/*
 * Flattens all the images pushed prior to calling this function into a single image and resets the stack.
 * Any PNG_Images not pushed with push_image_raw will be deallocated when you call this function.
 * Returns a pointer to a newly created PNG_Image which is the result of layer all the pushed images on top of each other.
 * The first image to be pushed will be the topmost layer, and the last image to be pushed will be the background layer.
 */
PNG_Image* get_flattened_image();

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////// IMAGE MANIPULATION ///////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * Blends an image with a specified background color by modifying the image's pixel data in place
 * Assumes pixels are represented as four consecutive bytes (RGBA: Red, Green, Blue, Alpha) in a flat array
 * Sets the opacity to full for every pixel
 */
PNG_Image* blend_images(const PNG_Image* const canvas, const PNG_Image* const image, int startX, int startY);

#endif