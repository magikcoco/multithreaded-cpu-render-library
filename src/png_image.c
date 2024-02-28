#include <png.h>
#include <setjmp.h>
#include <stdbool.h>
#include <string.h>
#include "png_image.h"

typedef struct {
    png_bytep data;
    size_t size;
    size_t current_pos;
} memory_reader_state;

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

void png_read_from_memory(png_structp png_ptr, png_bytep outBytes, png_size_t byteCountToRead) {
    memory_reader_state* state = (memory_reader_state*)png_get_io_ptr(png_ptr);
    if (state->current_pos + byteCountToRead <= state->size) {
        memcpy(outBytes, state->data + state->current_pos, byteCountToRead);
        state->current_pos += byteCountToRead;
    } else {
        png_error(png_ptr, "Read error in png_read_from_memory (not enough data)");
    }
}


PNG_Image* png_load_from_memory(unsigned char* memory, size_t memory_size) {
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        fprintf(stderr, "Failed to create PNG read struct\n");
        return NULL;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        fprintf(stderr, "Failed to create PNG info struct\n");
        png_destroy_read_struct(&png, NULL, NULL);
        return NULL;
    }

    if (setjmp(png_jmpbuf(png))) {
        fprintf(stderr, "Error during PNG initialization\n");
        png_destroy_read_struct(&png, &info, NULL);
        return NULL;
    }

    // Initialize memory_reader_state here to closely align with the XImage version
    memory_reader_state state = {memory, memory_size, 0};
    png_set_read_fn(png, &state, png_read_from_memory);

    png_read_info(png, info);
    apply_color_transformations(png, info);
    png_read_update_info(png, info);

    // Modifications for memory management to align with the XImage version
    PNG_Image* img = (PNG_Image*)malloc(sizeof(PNG_Image));
    if (!img) {
        fprintf(stderr, "Failed to allocate memory for PNG_Image\n");
        png_destroy_read_struct(&png, &info, NULL);
        return NULL;
    }

    img->width = png_get_image_width(png, info);
    img->height = png_get_image_height(png, info);
    img->bitDepth = png_get_bit_depth(png, info);
    img->colorType = png_get_color_type(png, info);
    size_t row_bytes = png_get_rowbytes(png, info);
    img->data = (unsigned char*)malloc(row_bytes * img->height);

    png_bytep* row_pointers = read_png_data(png, info, img->height);

    // Copy the image data directly into img->data
    for (int y = 0; y < img->height; y++) {
        memcpy(img->data + (y * row_bytes), row_pointers[y], row_bytes);
        free(row_pointers[y]); // Free each row pointer after copying
    }

    free(row_pointers); // Free the row pointers array
    png_destroy_read_struct(&png, &info, NULL);

    return img;
}

PNG_Image* png_load_from_file(char *filepath) {
    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        fprintf(stderr, "Could not open file %s for reading\n", filepath);
        return NULL;
    }

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        fclose(fp);
        return NULL;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_read_struct(&png, NULL, NULL);
        fclose(fp);
        return NULL;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return NULL;
    }

    png_init_io(png, fp);
    png_read_info(png, info);
    apply_color_transformations(png, info);
    png_read_update_info(png, info);

    int width = png_get_image_width(png, info);
    int height = png_get_image_height(png, info);
    png_bytep* row_pointers = read_png_data(png, info, height);

    // Assuming the image has been transformed to RGBA format
    PNG_Image* img = (PNG_Image*)malloc(sizeof(PNG_Image));
    if (!img) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return NULL; // Memory allocation failed
    }

    img->width = width;
    img->height = height;
    img->bitDepth = 8; // Assuming 8 bits per channel after transformations
    img->colorType = PNG_COLOR_TYPE_RGBA;
    img->data = (unsigned char*)malloc(width * height * 4); // 4 bytes per pixel (RGBA)

    // Copying data from row_pointers to img->data
    for (int y = 0; y < height; y++) {
        memcpy(img->data + (y * width * 4), row_pointers[y], width * 4);
    }

    // Cleanup
    for (int i = 0; i < height; i++) {
        free(row_pointers[i]);
    }
    free(row_pointers);
    png_destroy_read_struct(&png, &info, NULL);
    fclose(fp);

    // Here, img->data already contains the image in RGBA format,
    // so no need to call blend_with_background unless you want to change the background.

    return img;
}

PNG_Image* CreatePNG_Image(int width, int height, int bitDepth, int colorType) {
    // Allocate memory for the PNG_Image struct
    PNG_Image* img = (PNG_Image*)malloc(sizeof(PNG_Image));
    if (!img) {
        fprintf(stderr, "Failed to allocate memory for PNG_Image\n");
        return NULL;
    }

    // Initialize struct fields
    img->width = width;
    img->height = height;
    img->bitDepth = bitDepth;
    img->colorType = colorType;

    // Calculate the number of bytes needed for the image data
    // Assuming 4 channels (RGBA) for simplicity; adjust based on colorType if necessary
    size_t dataSize = width * height * 4; // 4 bytes per pixel for RGBA

    // Allocate memory for the image data
    img->data = (unsigned char*)malloc(dataSize);
    if (!img->data) {
        fprintf(stderr, "Failed to allocate memory for image data\n");
        free(img); // Free the previously allocated PNG_Image struct
        return NULL;
    }

    // Optionally, initialize the image data to zero (transparent black)
    memset(img->data, 0, dataSize);

    return img;
}

void DestroyPNG_Image(PNG_Image* img) {
    if (img != NULL) {
        // Free the image data if it exists
        if (img->data != NULL) {
            free(img->data);
            img->data = NULL; // Avoid dangling pointer
        }
        // Free the image struct itself
        free(img);
        img = NULL; // Avoid dangling pointer
    }
}