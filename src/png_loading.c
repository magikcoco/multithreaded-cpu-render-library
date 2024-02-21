#include <png.h>
#include <stdbool.h> // bool type in function signatures
#include <stdint.h> // uint32_t in the is_big_endian function
#include <stdio.h> // fopen, fclose, perror
#include <stdlib.h> // malloc, free
#include <string.h> // memcpy
#include <X11/Xlib.h>
#include "png_loading.h"

// Define the rgba_color structure
typedef struct {
    png_byte red, green, blue, alpha;
} rgba_color;

typedef struct {
    png_bytep data;
    size_t size;
    size_t current_pos;
} memory_reader_state;

bool initialize_png_reader(const char *filepath, FILE **fp, png_structp *png_ptr, png_infop *info_ptr) {
    // Open the file
    *fp = fopen(filepath, "rb");

    // Check if the file was successfully opened
    if (!*fp) {
        perror("Error opening file");
        return false;
    }

    // Initialize libpng read structure
    *png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    // Check if the read struct was created successfully
    if (!*png_ptr) {
        perror("Error creating png read struct");
        fclose(*fp);
        return false;
    }

    // Create PNG info struct
    *info_ptr = png_create_info_struct(*png_ptr);

    // Check if the PNG info struct was created successfully
    if (!*info_ptr) {
        perror("Error creating png info struct");
        png_destroy_read_struct(png_ptr, NULL, NULL);
        fclose(*fp);
        return false;
    }

    // Setup setjmp for handling libpng errors, if an error occurs in a libpng function after this it will jump back here
    if (setjmp(png_jmpbuf(*png_ptr))) {
        perror("Error during png init_io");
        png_destroy_read_struct(png_ptr, info_ptr, NULL);
        fclose(*fp);
        return false;
    }

    // Initializes the I/O for the libpng file reading process using the previously opened file.
    png_init_io(*png_ptr, *fp);
    return true;
}

void apply_color_transformations(png_structp png, png_infop info) {
    png_byte color_type = png_get_color_type(png, info);
    png_byte bit_depth = png_get_bit_depth(png, info);

    if (bit_depth == 16) {
        png_set_strip_16(png);
    }

    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png);
    }

    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
        png_set_expand_gray_1_2_4_to_8(png);
    }

    if (png_get_valid(png, info, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png);
    }

    if ((color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_PALETTE) && !(color_type & PNG_COLOR_MASK_ALPHA)) {
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    }

    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png);
    }
}

png_bytep *read_png_data(png_structp png, png_infop info, int height) {
    png_bytep *row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height);
    for(int y = 0; y < height; y++) {
        row_pointers[y] = (png_byte*)malloc(png_get_rowbytes(png, info));
    }

    png_read_image(png, row_pointers);
    return row_pointers;
}

png_bytep create_contiguous_image_data(png_structp png, png_infop info, png_bytep *row_pointers, int height) {
    // Allocate memory for the entire image
    png_bytep img_data = (png_bytep)malloc(height * png_get_rowbytes(png, info));

    for (int y = 0; y < height; y++) {
        memcpy(img_data + y * png_get_rowbytes(png, info), row_pointers[y], png_get_rowbytes(png, info));
        free(row_pointers[y]);
    }

    return img_data;
}

XImage *create_ximage_from_data(Display *display, png_bytep img_data, int width, int height) {
    Visual *visual = DefaultVisual(display, 0);
    return XCreateImage(display, visual, DefaultDepth(display, 0), ZPixmap, 0, (char*)img_data, width, height, 32, 0);
}

int is_big_endian(void) {
    union {
        uint32_t i;
        char c[4];
    } bint = {0x01020304};

    return bint.c[0] == 1; 
}

int system_byte_order_differs_from_png(void) {
    return !is_big_endian(); // Returns 1 if system is little-endian, 0 if big-endian
}

void swap_byte_order(png_bytep row, int width) {
    for (int x = 0; x < width; x++) {
        png_bytep pixel = &row[x * 4]; // 4 bytes per pixel (RGBA)
        png_byte temp = pixel[0];
        pixel[0] = pixel[2];
        pixel[2] = temp;
        // Alpha channel (pixel[3]) remains unchanged
    }
}

void blend_with_background(png_bytep img_data, int width, int height, rgba_color background_color) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            png_bytep pixel = &img_data[(y * width + x) * 4]; // 4 bytes per pixel (RGBA)

            // Alpha blending formula: new_color = alpha * foreground_color + (1 - alpha) * background_color
            double alpha = pixel[3] / 255.0; // Normalize alpha to [0, 1]
            pixel[0] = (png_byte)(alpha * pixel[0] + (1 - alpha) * background_color.red);
            pixel[1] = (png_byte)(alpha * pixel[1] + (1 - alpha) * background_color.green);
            pixel[2] = (png_byte)(alpha * pixel[2] + (1 - alpha) * background_color.blue);
            // Optionally, set the alpha to full opacity
            pixel[3] = 0xFF;
        }
    }
}

XImage *load_png_from_file(Display *display, char *filepath) {
    FILE *fp;
    png_structp png;
    png_infop info;

    if (!initialize_png_reader(filepath, &fp, &png, &info)) {
        return NULL; // Initialization failed, return early
    }

    png_read_info(png, info);
    
    apply_color_transformations(png, info);

    png_read_update_info(png, info);

    int height = png_get_image_height(png, info);
    int width = png_get_image_width(png, info);
    png_bytep *row_pointers = read_png_data(png, info, height);

    // Perform byte order swapping if necessary
    if (system_byte_order_differs_from_png()) {
        for (int y = 0; y < height; y++) {
            swap_byte_order(row_pointers[y], width);
        }
    }

    fclose(fp);

    png_bytep img_data = create_contiguous_image_data(png, info, row_pointers, height);

    // Perform alpha blending with the background color
    rgba_color background_color = {0xFF, 0xFF, 0xFF, 0xFF}; // Example: white background
    blend_with_background(img_data, width, height, background_color);

    free(row_pointers); // Free the row pointers array

    // Create the XImage using the contiguous block
    XImage *img = create_ximage_from_data(display, img_data, width, height);

    return img;
}

void png_read_from_memory(png_structp png_ptr, png_bytep outBytes, png_size_t byteCountToRead) {
    memory_reader_state* state = (memory_reader_state*)png_get_io_ptr(png_ptr);
    if (state->current_pos + byteCountToRead <= state->size) {
        memcpy(outBytes, state->data + state->current_pos, byteCountToRead);
        state->current_pos += byteCountToRead;
    } else {
        png_error(png_ptr, "Read error in png_read_from_memory (not enough data)");
    }
}

XImage *load_png_from_memory(Display *display, png_bytep memory, size_t memory_size) {
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        return NULL; // Failed, return early
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        perror("Error creating png info struct");
        png_destroy_read_struct(&png, NULL, NULL);
        return NULL;
    }

    if (setjmp(png_jmpbuf(png))) {
        perror("Error during png init_io");
        png_destroy_read_struct(&png, &info, NULL);
        return NULL;
    }

    memory_reader_state state = {memory, memory_size, 0};
    png_set_read_fn(png, &state, png_read_from_memory);

    png_read_info(png, info);
    apply_color_transformations(png, info);
    png_read_update_info(png, info);

    int height = png_get_image_height(png, info);
    int width = png_get_image_width(png, info);
    png_bytep *row_pointers = read_png_data(png, info, height);

    // Perform byte order swapping if necessary
    if (system_byte_order_differs_from_png()) {
        for (int y = 0; y < height; y++) {
            swap_byte_order(row_pointers[y], width);
        }
    }

    png_bytep img_data = create_contiguous_image_data(png, info, row_pointers, height);

    // Perform alpha blending with the background color
    rgba_color background_color = {0xFF, 0xFF, 0xFF, 0xFF}; // Example: white background
    blend_with_background(img_data, width, height, background_color);

    free(row_pointers); // Free the row pointers array

    // Create the XImage using the contiguous block
    XImage *img = create_ximage_from_data(display, img_data, width, height);

    return img;
}