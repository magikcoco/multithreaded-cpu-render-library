#include "compositing.h"

/*
 * Holds an image and where it should go in the flattened image
 */
typedef struct {
    const PNG_Image *const image;
    const int x;
    const int y;
} PNG_Image_With_Loc;

/*
 * Very basic stack for holding PNG_Image_With_Loc in the proper order
 * The last image to be pushed will be the first image used, becoming the background
 */
typedef struct {
    PNG_Image_With_Loc** items;
    int capacity;
    int top;
} PNG_Image_Stack;

PNG_Image_Stack global_stack = {NULL, 0, -1}; //TODO: can this be threadlocal?
pthread_mutex_t stack_lock = PTHREAD_MUTEX_INITIALIZER;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////// HELPER FUNCTIONS ///////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * Initializes a PNG_Image_With_Loc struct.
 * This function violates the constness of the PNG_Image_With_Loc struct in order to dynamically allocate it and also gives it's fields values.
 */
PNG_Image_With_Loc* create_png_image_with_loc(const PNG_Image *const image, const int x, const int y) {
    // Allocate memory for PNG_Image_With_Loc
    PNG_Image_With_Loc* immutable = (PNG_Image_With_Loc*)malloc(sizeof(PNG_Image_With_Loc));
    if (immutable) {
        // Cast away the const-ness of the struct pointer to allow initialization
        PNG_Image_With_Loc* nonimm = (void*)immutable;

        // Since the fields themselves are const, it is necessary to cast the address of each field
        // and then use the dereferenced pointer to assign a value.
        *(const PNG_Image**)(&nonimm->image) = image;
        *(int*)(&nonimm->x) = x;
        *(int*)(&nonimm->y) = y;
    }
    return immutable;
}

/* 
 * Pops an item of the top of the stack.
 */
PNG_Image_With_Loc* pop_image() {
    // This function isnt exposed because the PNG_Image_With_Loc struct is not exposed.
    if (global_stack.top == -1) {
        // Stack is empty, nothing to pop
        pthread_mutex_unlock(&stack_lock);
        return NULL;
    }

    // Retrieve the top item from the stack
    PNG_Image_With_Loc* item = global_stack.items[global_stack.top];
    
    // Decrease the stack top index
    global_stack.top--;

    // Return the popped item
    return item;
}

/*
 * Initializes the fields of the global_stack, dynamically allocates memory for the items based on the given inital capacity.
 */
void init_image_stack(int capacity) {
    global_stack.items = (PNG_Image_With_Loc**)malloc(sizeof(PNG_Image_With_Loc*) * capacity);
    global_stack.capacity = capacity;
    global_stack.top = -1;
}

/*
 * Deallocates everything except the included PNG_Image* entries in the PNG_Image_With_Loc struct (separate function).
 * Resets everything to a default value.
 */
void reset_image_stack(){
    PNG_Image_With_Loc* current;
    while((current = pop_image())){
        if(current) free(current);
    }
    if(global_stack.items) free(global_stack.items);
    global_stack.items = NULL;
    global_stack.capacity = 0; // A capacity of 0 means that the stack will be resized next time an image is pushed
    global_stack.top = -1;
}

/*
 * Resizes the stack.
 */
void resize_stack() {
    //TODO: use a moving average instead
    int new_capacity = (global_stack.capacity + 1) * 2; // +1 to avoid a case where the capacity is 0, and stays 0
    // This will behave like malloc if globalstack.items is null
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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////// STACK FUNCTIONS ///////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * Mark the given image to be rendered in a single flat image, obtained by calling get_flattened_image().
 * Images pushed sooner will overlap with and draw on top of images pushed later.
 * Images are rendered at the given X & Y coordinates, where the top left corner is (0, 0). X increases going right and Y increases going down.
 * The last image to be pushed is used as the background, and pixels that fall outside of it's boundaries will be discarded.
 * Images pushed in this way are not deallocated.
 */
void push_image_raw(const PNG_Image *const image, int x, int y) {
    pthread_mutex_lock(&stack_lock);

    if (global_stack.top == global_stack.capacity - 1) {
        // Resize the stack if it is full
        resize_stack();
    }
    
    global_stack.items[++global_stack.top] = create_png_image_with_loc(image, x, y);

    pthread_mutex_unlock(&stack_lock);
}

/*
 * Mark the given image to be rendered in a single flat image, obtained by calling get_flattened_image().
 * Images pushed sooner will overlap with and draw on top of images pushed later.
 * Images are rendered at the given X & Y coordinates, where the top left corner is (0, 0). X increases going right and Y increases going down.
 * The last image to be pushed is used as the background, and pixels that fall outside of it's boundaries will be discarded.
 * Images pushed in this way are automatically deallocated after get_flattened_image() is called.
 */
void push_image(PNG_Image* image, int x, int y) {
    // TODO: Implement this
    // Push the image and mark it for later deletion
}

/*
 * Flattens all the images pushed prior to calling this function into a single image and resets the stack.
 * Any PNG_Images not pushed with push_image_raw will be deallocated when you call this function.
 * Returns a pointer to a newly created PNG_Image which is the result of layer all the pushed images on top of each other.
 * The first image to be pushed will be the topmost layer, and the last image to be pushed will be the background layer.
 */
PNG_Image* get_flattened_image() {
    pthread_mutex_lock(&stack_lock);
    
    if (global_stack.top < 0) { // No images to flatten
        pthread_mutex_unlock(&stack_lock);
        return NULL;
    }

    // Create a new PNG_Image to serve as the canvas
    // Assume background dimensions are set by the first image in the stack
    PNG_Image_With_Loc* current;
    PNG_Image* flattened = NULL;
    // Iterate through the stack
    while((current = pop_image())){
        // For each image, blend it onto the flattened canvas considering its X, Y coordinates
        if(!flattened){
            flattened = png_copy_image(current->image);
        } else {
            PNG_Image* temp = blend_images(flattened, current->image, current->x, current->y);
            png_destroy_image(&flattened);
            flattened = temp;
        }
        free(current); // Don't need this anymore
    }

    reset_image_stack();
    
    pthread_mutex_unlock(&stack_lock);
    
    return flattened;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////// IMAGE MANIPULATION ///////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * Blends the given image onto the given canvas, dropping any pixels which fall out of bounds.
 * Specified X & Y determine where the topleft corner of the image is drawn on the canvas.
 * Returns a pointer to a newly created PNG_Image
 */
PNG_Image* blend_images(const PNG_Image* const canvas, const PNG_Image* const image, int image_x, int image_y) {
    // Make a deep copy of the canvas to use as a base for blending
    PNG_Image* result = png_copy_image(canvas);

    // Iterate over each pixel in the source image
    for (int y = 0; y < image->height; y++) {
        for (int x = 0; x < image->width; x++) {
            // Calculate the destination pixel's position on the canvas
            int dest_x = image_x + x;
            int dest_y = image_y + y;
            
            // Check if the pixel is within the canvas boundaries
            if (dest_x < 0 || dest_y < 0 || dest_x >= result->width || dest_y >= result->height) {
                continue; // Skip this pixel, it's outside the canvas
            }
            
            // Calculate the address of the current pixel in both the source and destination images
            png_bytep src_pixel = &image->data[(y * image->width + x) * 4]; // Source pixel in the image
            png_bytep dest_pixel = &result->data[(dest_y * result->width + dest_x) * 4]; // Destination pixel on the canvas copy
            
            // Perform alpha blending
            double alpha = src_pixel[3] / 255.0; // Normalize alpha to [0, 1]
            for (int i = 0; i < 3; i++) { // Blend RGB components
                dest_pixel[i] = (png_byte)(alpha * src_pixel[i] + (1 - alpha) * dest_pixel[i]);
            }
            // Assuming the destination pixel's alpha should also consider the source pixel's alpha
            dest_pixel[3] = (png_byte)(alpha * src_pixel[3] + (1 - alpha) * dest_pixel[3]);
        }
    }
    
    return result;
}
