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
    }

    // Now free the array of pointers itself
    free(global_stack.items);
    
    reset_image_stack();

    pthread_mutex_unlock(&stack_lock);
}

void resize_stack() {
    int new_capacity = (global_stack.capacity + 1) * 2; //+1 to avoid a case where the capacity is 0, and stays 0
    PNG_Image** new_items = (PNG_Image**)realloc(global_stack.items, sizeof(PNG_Image*) * new_capacity);

    if (new_items) {
        global_stack.items = new_items;
        global_stack.capacity = new_capacity;
    } else {
        // Handle reallocation failure
        printf("Error resizing stack.\n");
        exit(EXIT_FAILURE); //TODO: too drastic, change this to something more graceful
        // Fail reasons:
        // Insufficient memory
        // Invalid pointer
        // Zero size
        // Fragmentation
        // System-specific limits
    }
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