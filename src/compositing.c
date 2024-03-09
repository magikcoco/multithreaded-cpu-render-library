#include "compositing.h"

typedef struct {
    PNG_Image* image;
    int x;
    int y;
} PNG_Image_With_Loc;

typedef struct {
    PNG_Image_With_Loc** items;
    int capacity;
    int top;
} PNG_Image_Stack;

PNG_Image_Stack global_stack = {NULL, 0, -1};
pthread_mutex_t stack_lock = PTHREAD_MUTEX_INITIALIZER;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////// HELPER FUNCTIONS ///////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void init_image_stack(int capacity) {
    global_stack.items = (PNG_Image_With_Loc**)malloc(sizeof(PNG_Image_With_Loc*) * capacity);
    global_stack.capacity = capacity;
    global_stack.top = -1;
}

void reset_image_stack(){
    global_stack.items = NULL;
    global_stack.capacity = 0;
    global_stack.top = -1;
}

void resize_stack() {
    int new_capacity = (global_stack.capacity + 1) * 2; //+1 to avoid a case where the capacity is 0, and stays 0
    PNG_Image_With_Loc** new_items = (PNG_Image_With_Loc**)realloc(global_stack.items, sizeof(PNG_Image_With_Loc*) * new_capacity);

    if (new_items) {
        global_stack.items = new_items;
        global_stack.capacity = new_capacity;
    } else {
        // Handle reallocation failure
        exit(EXIT_FAILURE); //TODO: too drastic, change this to something more graceful
        // Fail reasons:
        // Insufficient memory
        // Invalid pointer
        // Zero size
        // Fragmentation
        // System-specific limits
    }
}

//TODO: refactor clearing the stack, it works weird, idk why

void clear_image_stack() {
    pthread_mutex_lock(&stack_lock);
    free(global_stack.items);
    reset_image_stack();
    pthread_mutex_unlock(&stack_lock);
}

void destroy_image_stack() {
    pthread_mutex_lock(&stack_lock);

    // Iterate through the stack and deallocate each PNG_Image
    for(int i = 0; i <= global_stack.top; i++) {
        DestroyPNG_Image(&global_stack.items[i]->image);
        free(global_stack.items[i]); // Free the struct itself
    }

    // Now free the array of pointers itself
    free(global_stack.items);
    
    reset_image_stack();

    pthread_mutex_unlock(&stack_lock);
}

void push_image(PNG_Image* image, int x, int y) {
    pthread_mutex_lock(&stack_lock);

    if (global_stack.top == global_stack.capacity - 1) {
        // Resize the stack if it is full
        resize_stack(); // Ensure resize_stack is also updated to handle PNG_ImageWithCoords
    }
    
    // Allocate memory for a new PNG_ImageWithCoords
    PNG_Image_With_Loc* new_image = (PNG_Image_With_Loc*)malloc(sizeof(PNG_Image_With_Loc));
    if (!new_image) {
        // Handle memory allocation failure
        pthread_mutex_unlock(&stack_lock);
        return; // TODO: error code return
    }
    
    new_image->image = image;
    new_image->x = x;
    new_image->y = y;
    
    // Add the new PNG_ImageWithCoords to the stack
    global_stack.items[++global_stack.top] = new_image;

    pthread_mutex_unlock(&stack_lock);
}

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

void blend_image_onto_canvas(png_bytep canvas_data, PNG_Image_With_Loc* img_with_coords, int canvas_width, int canvas_height) {
    PNG_Image* img = img_with_coords->image;
    int imgWidth = img->width;
    int imgHeight = img->height;
    int startX = img_with_coords->x;
    int startY = img_with_coords->y;
    
    for (int y = 0; y < imgHeight; y++) {
        for (int x = 0; x < imgWidth; x++) {
            // Calculate the destination pixel's position on the canvas
            int destX = startX + x;
            int destY = startY + y;
            
            // Check if the pixel is within the canvas boundaries
            if (destX < 0 || destY < 0 || destX >= canvas_width || destY >= canvas_height) {
                continue; // Skip this pixel, it's outside the canvas
            }
            
            // Calculate the address of the current pixel in both the source and destination images
            png_bytep src_pixel = &img->data[(y * imgWidth + x) * 4]; // Source pixel in the image being processed
            png_bytep dest_pixel = &canvas_data[(destY * canvas_width + destX) * 4]; // Destination pixel on the canvas
            
            // Perform alpha blending
            double alpha = src_pixel[3] / 255.0; // Normalize alpha to [0, 1]
            for (int i = 0; i < 3; i++) { // Blend RGB components
                dest_pixel[i] = (png_byte)(alpha * src_pixel[i] + (1 - alpha) * dest_pixel[i]);
            }
            // Set alpha to full opacity
            dest_pixel[3] = 0xFF;
        }
    }
}

PNG_Image* flatten_stack_into_image() {
    pthread_mutex_lock(&stack_lock);
    
    if (global_stack.top < 0) { // No images to flatten
        pthread_mutex_unlock(&stack_lock);
        return NULL;
    }
    
    // Assume background dimensions are set by the first image in the stack
    PNG_Image* background = global_stack.items[0]->image;
    int bgWidth = background->width;
    int bgHeight = background->height;
    
    // Create a new PNG_Image to serve as the canvas
    //TODO: Change this because it makes the background black instead of white
    PNG_Image* flattened = CreatePNG_Image(bgWidth, bgHeight, 8, PNG_COLOR_TYPE_RGBA);
    
    // Iterate through the stack from top (background) to bottom
    for (int i = global_stack.top; i >= 0; i--) {
        PNG_Image_With_Loc* current = global_stack.items[i];
        // For each image, blend it onto the flattened canvas considering its X, Y coordinates
        // Modify blend_with_background or create a new function to handle this
        blend_image_onto_canvas(flattened->data, current, bgWidth, bgHeight);
    }
    
    pthread_mutex_unlock(&stack_lock);
    
    return flattened;
}