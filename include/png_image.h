#ifndef PNG_IMAGE_H
#define PNG_IMAGE_H

#include <stdlib.h>

// Struct to represent an RGBA image
typedef struct PNG_Image {
    int width;      // Image width
    int height;     // Image height
    int bitDepth;   // Bit depth per channel
    int colorType;  // Color type (e.g., RGBA, RGB, Grayscale)
    unsigned char* data; // Pointer to the image data
} PNG_Image;

PNG_Image* png_load_from_memory(unsigned char* memory, size_t memory_size);
PNG_Image* png_load_from_file(char *filepath);
PNG_Image* CreatePNG_Image(int width, int height, int bitDepth, int colorType);
void DestroyPNG_Image(PNG_Image** imgPtr);

#endif // SIMPLE_IMAGE_H