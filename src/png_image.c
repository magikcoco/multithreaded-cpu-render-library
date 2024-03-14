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

/*
 * Extract color components from RGBA hex code
 */
int extract_color_component(uint32_t rgba, int shift) {
    return (rgba >> shift) & 0xFF;
}

/*
 * A series of transformations on a PNG image's color and bit depth to standardize its format to RGBA 8 bits
 * Uses libpng to manipulate PNG image data directly
 */
void apply_color_transformations(png_structp png, png_infop info) {
    // Retrieve the color type of the image (e.g., RGB, grayscale, palette)
    png_byte color_type = png_get_color_type(png, info);
    // Retrieve the color type of the image (e.g., RGB, grayscale, palette)
    png_byte bit_depth = png_get_bit_depth(png, info);

    // If the image's bit depth is 16, reduce it to 8 by stripping the least significant bits
    if (bit_depth == 16) {
        png_set_strip_16(png);
    }

    // If the image uses a color palette, convert it to a direct RGB representation
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png);
    }

    // If the image is grayscale and has a bit depth less than 8, expand it to 8 bits
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
        png_set_expand_gray_1_2_4_to_8(png);
    }

    // If the image has transparency information (tRNS), convert it to an alpha channel
    if (png_get_valid(png, info, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png);
    }

    // If the image is RGB, grayscale, or palette-based without an alpha channel, add a full alpha channel
    if ((color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_PALETTE) && !(color_type & PNG_COLOR_MASK_ALPHA)) {
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    }

    // If the image is grayscale (with or without alpha), convert it to RGB (preserving any alpha channel)
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png);
    }
}

/*
 * Reads image data from a PNG file into a dynamically allocated array of row pointers, each pointing to the pixel data of a single row
 */
png_bytep *allocate_and_load_png_image_rows(png_structp png, png_infop info, int height) {
    // Dynamically allocate memory for an array of pointers to each row of the image
    png_bytep *row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height);

    // Iterate over each row of the image to allocate memory for storing pixel data
    for(int y = 0; y < height; y++) {
        // Allocate memory for each row based on the number of bytes per row
        row_pointers[y] = (png_byte*)malloc(png_get_rowbytes(png, info));
    }

    // Read the entire image into the memory allocated for the row pointers
    png_read_image(png, row_pointers);

    // Return the array of row pointers containing the image data
    return row_pointers;
}

/*
 * Read PNG data from a memory buffer rather than from a file or stream
 */
void read_png_data_from_memory_buffer(png_structp png_ptr, png_bytep outBytes, png_size_t byteCountToRead) {
    // Retrieve the custom I/O state pointer set with png_set_read_fn()
    memory_reader_state* state = (memory_reader_state*)png_get_io_ptr(png_ptr);

    // Check if there is enough data left to read the requested number of bytes
    if (state->current_pos + byteCountToRead <= state->size) {
        // Copy the requested number of bytes from the memory buffer to the output buffer
        memcpy(outBytes, state->data + state->current_pos, byteCountToRead);

        // Update the current position in the memory buffer
        state->current_pos += byteCountToRead;
    } else {
        // If not enough data is available, trigger a libpng error
        png_error(png_ptr, "Read error in png_read_from_memory (not enough data)");
    }
}

/*
 * Allocates memory for the PNG_Image struct
 */
PNG_Image* create_empty_png_image_struct() {
    PNG_Image* img = (PNG_Image*)malloc(sizeof(PNG_Image));
    if (!img) {
        fprintf(stderr, "Failed to allocate memory for PNG_Image\n");
        return NULL;
    }
    // Initialize with default values or leave uninitialized to be set later
    img->data = NULL; // Will be allocated later
    return img;
}

/*
 * Loads a PNG image from a memory buffer into a custom PNG_Image structure
 */
PNG_Image* png_load_from_memory(const unsigned char *const memory, size_t memory_size) {
    // Create a PNG read structure required for processing PNG data.
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    // Check for failure to create the read struct
    if (!png) {
        // If the read structure couldn't be created, print an error and return NULL
        fprintf(stderr, "Failed to create PNG read struct\n");
        return NULL;
    }

    // Create a PNG info structure to hold the image information
    png_infop info = png_create_info_struct(png);

    // Check for failure to create the info struct
    if (!info) {
        // If the info structure couldn't be created, clean up and return NULL
        fprintf(stderr, "Failed to create PNG info struct\n");
        png_destroy_read_struct(&png, NULL, NULL);
        return NULL;
    }

    // Set up error handling. If an error occurs, control jumps to this point
    if (setjmp(png_jmpbuf(png))) {
        // On error, clean up and return NULL
        fprintf(stderr, "Error during PNG initialization\n");
        png_destroy_read_struct(&png, &info, NULL);
        return NULL;
    }

    // Make a copy of the memory parameter to preserve const-ness
    unsigned char* memory_copy = malloc(memory_size);
    if (!memory_copy) {
        // Handle memory allocation failure
        perror("Failed to allocate memory for data copy");
        exit(EXIT_FAILURE); //TODO: too sever handle this better
    }

    // Initialize memory_reader_state here to closely align with the XImage version
    memory_reader_state state = {memcpy(memory_copy, memory, memory_size), memory_size, 0};

    // Set the custom read function to use the memory reader state
    png_set_read_fn(png, &state, read_png_data_from_memory_buffer);

    // Read the PNG image info
    png_read_info(png, info);

    // Apply transformations to standardize the image data format
    apply_color_transformations(png, info);

    // Update the PNG structure with the transformations
    png_read_update_info(png, info);

    // Allocate memory for a PNG_Image structure to hold the image data
    PNG_Image* img = create_empty_png_image_struct();

    // Check for failure
    if (!img) {
        // If allocation fails, clean up and return NULL
        png_destroy_read_struct(&png, &info, NULL);
        return NULL;
    }

    // Set the image dimensions, bit depth, and color type from the PNG info structure
    img->width = png_get_image_width(png, info);
    img->height = png_get_image_height(png, info);

    // Calculate the number of bytes in a row of the image
    size_t row_bytes = png_get_rowbytes(png, info);

    // Allocate memory for the image data based on the number of bytes per row and the image height
    img->data = (unsigned char*)malloc(row_bytes * img->height);

    // Check for failure
    if (!img->data) {
        // If allocation fails, clean up and return NULL
        free(img);
        png_destroy_read_struct(&png, &info, NULL);
        return NULL;
    }

    // Read the image data into an array of row pointers
    png_bytep* row_pointers = allocate_and_load_png_image_rows(png, info, img->height);

    // Copy the image data from the row pointers directly into the img->data buffer
    for (int y = 0; y < img->height; y++) {
        memcpy(img->data + (y * row_bytes), row_pointers[y], row_bytes);
        // Free each row pointer after copying its data
        free(row_pointers[y]); // Free each row pointer after copying
    }

    // Cleanup
    free(row_pointers); // Free the row pointers array
    free(memory_copy);
    png_destroy_read_struct(&png, &info, NULL); // Clean up PNG read and info structures

    // Return the pointer to the PNG_Image structure containing the loaded image data
    return img;
}

/*
 * Loads a PNG image from a specified file path into a custom PNG_Image structure
 */
PNG_Image* png_load_from_file(const char *const filepath) {
    // Attempt to open the specified file in read-binary mode
    FILE *fp = fopen(filepath, "rb");

    // Check for failure to open file
    if (!fp) {
        // If the file couldn't be opened, print an error message and return NULL
        fprintf(stderr, "Could not open file %s for reading\n", filepath);
        return NULL;
    }

    // Create a PNG read structure needed for processing PNG files
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    // Check for failure to crate read struct
    if (!png) {
        // If the read structure couldn't be created, close the file and return NULL
        fclose(fp);
        return NULL;
    }

    // Create a PNG info structure to hold the image information
    png_infop info = png_create_info_struct(png);

    // Check for failure to create info struct
    if (!info) {
        // If the info structure couldn't be created, clean up and return NULL
        png_destroy_read_struct(&png, NULL, NULL);
        fclose(fp);
        return NULL;
    }

    // Set up error handling. If an error occurs, control jumps to this point
    if (setjmp(png_jmpbuf(png))) {
        // On error, clean up and return NULL
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return NULL;
    }

    // Initialize libpng's input/output to the opened file
    png_init_io(png, fp);

    // Read the PNG image info
    png_read_info(png, info);

    // Apply transformations to standardize the image data format
    apply_color_transformations(png, info);

    // Update the PNG structure with the transformations
    png_read_update_info(png, info);

    // Retrieve the image dimensions from the info structure
    int width = png_get_image_width(png, info);
    int height = png_get_image_height(png, info);

    // Allocate memory for a PNG_Image structure to store the image data
    PNG_Image* img = create_empty_png_image_struct();

    // Check for allocation failure
    if (!img) {
        // If memory allocation fails, clean up and return NULL
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return NULL; // Memory allocation failed
    }

    // Set the image properties and allocate memory for the image data
    img->width = width;
    img->height = height;

    // Allocate memory for data
    img->data = (unsigned char*)malloc(width * height * 4); // 4 bytes per pixel (RGBA)

    // Check for failure
    if (!img->data) {
        // If allocation fails, clean up and return NULL
        free(img);
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return NULL;
    }

    // Allocate memory and load the image data into an array of row pointers
    png_bytep* row_pointers = allocate_and_load_png_image_rows(png, info, height);

    // Copy the image data from the row pointers into the img->data buffer
    for (int y = 0; y < height; y++) {
        memcpy(img->data + (y * width * 4), row_pointers[y], width * 4);
    }
    
    // Clean up the memory allocated for the row pointers
    for (int i = 0; i < height; i++) {
        free(row_pointers[i]);
    }
    free(row_pointers);

    // Clean up other resources
    png_destroy_read_struct(&png, &info, NULL);
    fclose(fp);

    // Return the pointer to the PNG_Image structure containing the loaded image data
    return img;
}

/*
 * Creates and initializes a new PNG_Image structure, allocating memory for both the structure and its associated image data
 */
PNG_Image* png_create_image(int width, int height, uint32_t rgba) {
    if(rgba <= 0xFFFFFF && rgba > 0xFFFFF) {
        // No alpha given but valid RGB code, assume full opacity
        rgba = (rgba << 8) | 0xFF;
    } else if (rgba <= 0xFFFFFFF && rgba > 0xFFFFFF) {
        // Invalid alpha channel, assume full opacity
        rgba = (rgba >> 4); // Remove least significant digit
        rgba = (rgba << 8) | 0xFF; // Add full opacity alpha channel
    } else if(rgba <= 0xFFFFF) {
        // Invalid RGBA or RGB code, assume white
        rgba = 0xFFFFFF;
    }

    // Verify R, G, and B components are valid; if A is invalid, assume full opacity
    int red = extract_color_component(rgba, 24);
    int green = extract_color_component(rgba, 16);
    int blue = extract_color_component(rgba, 8);
    int alpha = extract_color_component(rgba, 0); // Extract Alpha; if invalid, will assume full opacity later

    // Allocate memory for the PNG_Image struct
    PNG_Image* img = create_empty_png_image_struct();
    if (!img) {
        // If memory allocation fails, return null
        return NULL;
    }

    // Initialize the structure fields with the provided parameters
    img->width = width;
    img->height = height;

    // Calculate the number of bytes needed for the image data
    size_t dataSize = width * height * 4; // 4 bytes per pixel for RGBA

    // Allocate memory for the image data
    img->data = (unsigned char*)malloc(dataSize);

    // Check for failure
    if (!img->data) {
        // If memory allocation for the image data fails, print an error
        fprintf(stderr, "Failed to allocate memory for image data\n");
        free(img); // Free the previously allocated PNG_Image struct
        return NULL;
    }

    // Initialize the image data to fully opaque white
    for (size_t i = 0; i < dataSize; i += 4) {
        img->data[i] = red;
        img->data[i + 1] = green;
        img->data[i + 2] = blue;
        img->data[i + 3] = alpha; // Check if alpha is unspecified or invalid, assume full opacity
    }

    // Return the pointer to the newly created PNG_Image structure
    return img;
}

/*
 * Creates a deep copy of a PNG_Image assuming RGBA format
 */
PNG_Image* png_copy_image(const PNG_Image *const source) {
    if (source == NULL) return NULL;

    // Allocate memory for the copy of the PNG_Image structure
    PNG_Image* copy = (PNG_Image*)malloc(sizeof(PNG_Image));
    if (copy == NULL) {
        fprintf(stderr, "Failed to allocate memory for PNG_Image copy\n");
        return NULL;
    }

    // Copy basic attributes directly
    copy->width = source->width;
    copy->height = source->height;

    // Since we're assuming RGBA format, we calculate the data size as width * height * 4
    size_t dataSize = source->width * source->height * 4; // 4 bytes per pixel for RGBA
    copy->data = (unsigned char*)malloc(dataSize);
    if (copy->data == NULL) {
        fprintf(stderr, "Failed to allocate memory for image data copy\n");
        free(copy); // Free the allocated PNG_Image structure if data allocation fails
        return NULL;
    }

    // Copy the image data
    memcpy(copy->data, source->data, dataSize);

    return copy;
}

/*
 * Safely deallocate memory used by a PNG_Image structure, including its image data, and then the structure itself
 */
void png_destroy_image(PNG_Image** imgPtr) {
    // First, check if the provided PNG_Image pointer is not NULL to avoid attempting to free a NULL pointer
    if (imgPtr != NULL && *imgPtr != NULL) {
        // Free the image data if it exists
        if ((*imgPtr)->data != NULL) {
            free((*imgPtr)->data);
            (*imgPtr)->data = NULL; // Avoid dangling pointer
        }
        // Free the image struct itself
        free(*imgPtr);
        // Set the user's pointer to NULL to avoid dangling pointer usage outside this function
        *imgPtr = NULL;
    }
}