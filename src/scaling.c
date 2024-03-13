#include <math.h>
#include <stdlib.h>
#include <stdio.h> // perror
#include "scaling.h"

//TODO: Bicubic interpolation
//TODO: Lanczos Resampling
//TODO: Box Sampling

/*
 * extracts the red component of a color
 * uses a bitwise AND operation with the mask 0x00FF0000 to isolate the bits representing the red component in the color
 * shifts these bits 16 places to the right, resulting in an 8-bit value (0-255) that corresponds to the red intensity
 */
unsigned int getR(unsigned int color) {
    return (color & 0x00FF0000) >> 16;
}

/*
 * extracts the green component of a color
 * uses a bitwise AND operation with the mask 0x0000FF00 to isolate the bits representing the green component in the color
 * shifts these bits 8 places to the right, resulting in an 8-bit value (0-255) that corresponds to the red intensity
 */
unsigned int getG(unsigned int color) {
    return (color & 0x0000FF00) >> 8;
}

/*
 * extracts the green component of a color
 * uses a bitwise AND operation with the mask 0x000000FF to isolate the bits representing the green component in the color
 * no bit shift performed, resulting in an 8-bit value (0-255) that corresponds to the red intensity
 */
unsigned int getB(unsigned int color) {
    return (color & 0x000000FF);
}

/*
 * scales an image to a new width and height using nearest neighbor scaling
 * returns a newly created PNG_Image struct
 * does not deallocate the original PNG_Image at all
 */
PNG_Image* nearest_neighbor_scale(const PNG_Image *const orig, int new_width, int new_height) {
    // Calculate the ratio of old width to new width, shifted left by 16 bits for fixed-point arithmetic, and add 1 for rounding
    int x_ratio = (int)((orig->width << 16) / new_width) + 1;
    // Calculate the ratio of old height to new height, similarly adjusted for fixed-point arithmetic
    int y_ratio = (int)((orig->height << 16) / new_height) + 1;

    // Create a new PNG_Image structure for the scaled image with the specified dimensions and original image's bit depth and color type
    PNG_Image* scaled = png_create_image(new_width, new_height);

    // Check if the scaled image and its data were successfully created; if not, print an error and return the original image
    if (!scaled || !scaled->data) {
        // If only the scaled image structure was created, free it to avoid memory leaks
        perror("Nearest Neighbor Scaling");
        if (scaled) png_destroy_image(&scaled);
        return NULL;
    }

    // Loop over each pixel in the new (scaled) image height
    for (int i = 0; i < new_height; i++) {
        // Loop over each pixel in the new (scaled) image width
        for (int j = 0; j < new_width; j++) {
            // Calculate the corresponding x coordinate in the original image, using fixed-point math and shifting back
            int x2 = ((j * x_ratio) >> 16);
            // Calculate the corresponding y coordinate in the original image, using fixed-point math and shifting back
            int y2 = ((i * y_ratio) >> 16);

            // Ensure that the calculated coordinates do not exceed the original image boundaries
            x2 = (x2 >= orig->width) ? orig->width - 1 : x2;
            y2 = (y2 >= orig->height) ? orig->height - 1 : y2;

            // Copy the pixel from the original image at (x2, y2) to the current position in the scaled image
            unsigned int orig_pixel = *((unsigned int*)(orig->data + y2 * (orig->width * 4) + x2 * 4));
            // Place the copied pixel into the correct position in the scaled image's data buffer
            *((unsigned int*)(scaled->data + i * (scaled->width * 4) + j * 4)) = orig_pixel;
        }
    }

    return scaled;
}

/*
 * scales an image to a new width and height using nearest bilinear interpolation scaling
 * returns a newly created PNG_Image struct
 * does not deallocate the original PNG_Image at all
 */
PNG_Image* bilinear_interpolation_scale(const PNG_Image *const orig, int new_width, int new_height) {
    // Create a new PNG_Image structure for the scaled image
    PNG_Image* scaled = png_create_image(new_width, new_height);

    // Check if memory allocation for the scaled image data was successful
    if (scaled->data == NULL) {
        perror("Bilinear interpolation");
        png_destroy_image(&scaled);
        return NULL; // Return NULL on failure
    }

    // Calculate the ratio of the old dimensions to the new dimensions minus one to avoid accessing out of bounds
    double x_ratio = (double)(orig->width - 1) / new_width;
    double y_ratio = (double)(orig->height - 1) / new_height;

    // Variables to store the exact positions where the pixel values will be interpolated
    double px, py;

    // Iterate over each pixel in the new image's height
    for (int i = 0; i < new_height; i++) {
        // Iterate over each pixel in the new image's width
        for (int j = 0; j < new_width; j++) {
            // Calculate the x and y positions on the original image to start interpolation
            px = floor(j * x_ratio);
            py = floor(i * y_ratio);

            // Calculate the fractional parts of the x and y positions
            double fx = j * x_ratio - px;
            double fy = i * y_ratio - py;

            // Boundary checks
            // Ensure the positions do not exceed the original image boundaries
            int px1 = (px + 1 < orig->width) ? px + 1 : px;
            int py1 = (py + 1 < orig->height) ? py + 1 : py;

            // Retrieve the pixel values from the four closest neighbors
            int p1 = *((unsigned int*)(orig->data + (int)py * (orig->width * 4) + (int)px * 4));
            int p2 = *((unsigned int*)(orig->data + (int)py * (orig->width * 4) + (int)px1 * 4));
            int p3 = *((unsigned int*)(orig->data + (int)py1 * (orig->width * 4) + (int)px * 4));
            int p4 = *((unsigned int*)(orig->data + (int)py1 * (orig->width * 4) + (int)px1 * 4));

            // Interpolate the red, green, and blue components separately based on their weights
            unsigned int r = (unsigned int)(getR(p1) * (1 - fx) * (1 - fy) + getR(p2) * fx * (1 - fy) + getR(p3) * (1 - fx) * fy + getR(p4) * fx * fy);
            unsigned int g = (unsigned int)(getG(p1) * (1 - fx) * (1 - fy) + getG(p2) * fx * (1 - fy) + getG(p3) * (1 - fx) * fy + getG(p4) * fx * fy);
            unsigned int b = (unsigned int)(getB(p1) * (1 - fx) * (1 - fy) + getB(p2) * fx * (1 - fy) + getB(p3) * (1 - fx) * fy + getB(p4) * fx * fy);

            // Combine the interpolated components back into a single pixel value and store it in the scaled image
            *((unsigned int*)(scaled->data + i * (scaled->width * 4) + j * 4)) = (r << 16) | (g << 8) | b;
        }
    }

    // Return the pointer to the scaled image
    return scaled;
}