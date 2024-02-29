#include "compositing.h"

/*
 * Blends an image with a specified background color by modifying the image's pixel data in place
 * Assumes pixels are represented as four consecutive bytes (RGBA: Red, Green, Blue, Alpha) in a flat array
 * Sets the opacity to full for every pixel
 */
void blend_with_background(png_bytep img_data, int width, int height, rgba_color background_color) {
    for (int y = 0; y < height; y++) { // Row
        for (int x = 0; x < width; x++) { // Column
            // Calculate the address of the current pixel and get its pointer
            png_bytep pixel = &img_data[(y * width + x) * 4]; // 4 bytes per pixel (RGBA)

            // Alpha blending formula: new_color = alpha * foreground_color + (1 - alpha) * background_color
            double alpha = pixel[3] / 255.0; // Normalize alpha to [0, 1]
            // Blend the red component of the pixel with the red component of the background color
            pixel[0] = (png_byte)(alpha * pixel[0] + (1 - alpha) * background_color.red);
            // Blend the green component of the pixel with the green component of the background color
            pixel[1] = (png_byte)(alpha * pixel[1] + (1 - alpha) * background_color.green);
            // Blend the blue component of the pixel with the blue component of the background color
            pixel[2] = (png_byte)(alpha * pixel[2] + (1 - alpha) * background_color.blue);
            // Optionally, set the alpha to full opacity
            pixel[3] = 0xFF;
        }
    }
}