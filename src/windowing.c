#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xatom.h> //Atom handling for close event
#include <X11/keysym.h> //Key handlers
#include <X11/Xutil.h> // XDestroyImage
#include "compositing.h"
#include "logo.h"
#include "scaling.h"
#include "task_queue.h"
#include "windowing.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////// VARIABLES ///////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// GUI Resources
// Should only be directly accessed in the GUI thread
Display* d; // Connection to X Server, GUI thread exclusive resource
Window w; // Window, GUI thread exclusive
PNG_Image* image; // Image to display in the window

// Window parameters
// Should only be directly accessed in the GUI thread
int x = 50;
int y = 50;
int width = 250;
int height = 250;
int border_width = 1;

// Keyhandler
#define MAX_KEYS 256 // "Why would you need more keys than this, right?" - last words in progress, I'm sure
KeyHandler key_handlers[MAX_KEYS]; // Array to store handlers for each key, GUI thread exclusive

// Scaling booleans
atomic_bool use_nn = false; // Indicates if nearest neighbor is in use
atomic_bool use_bli = false; // Indicates if bilinear interpolation is in use

// Shutdown and startup booleans
atomic_bool shutdown_flag = false; // When true triggers shutdown of GUI
atomic_bool start_flag = false; // When false indicates that the GUI has not started

// Mouse position
// Needed to work around other threads accessing GUI resources like the display
atomic_int global_mouse_x = ATOMIC_VAR_INIT(0);
atomic_int global_mouse_y = ATOMIC_VAR_INIT(0);

// Multithreading variables
pthread_t thread_id; // Thread id for the GUI thread
TaskQueue queue; // Task queue for when functions are called outside the GUI thread
_Thread_local int is_gui_thread = 0; // 1 if GUI thread, 0 otherwise

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////// HELPER FUNCTIONS ///////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * handles what to do when SIGTERM is received
 */
void termination_handler(){
    atomic_store(&shutdown_flag, true);
}

/*
 * Function to mark the current thread as the GUI thread
 */
void mark_as_gui_thread() {
    is_gui_thread = 1;
}

/*
 * Function to check if in GUI thread, just returns is_gui_thread variable
 */ 
int in_gui_thread() {
    return is_gui_thread;
}

/*
 * Processes all currently queued GUI tasks
 * The queue implementation handles threadsafety for itself
 */
void process_gui_tasks() {
    void (*function)(void*);
    void* arg;
    while (queue_dequeue(&queue, &function, &arg)) {
        function(arg); // Execute the dequeued task
    }
}

/*
 * returns an XImage copy of the PNG_Image given as an argument
 */
XImage* png_image_to_ximage(PNG_Image* p) {
    // Check if the display (d) or PNG_Image (p) pointer is NULL, return NULL to indicate failure
    if (!d || !p) return NULL;

    // Get the color depth of the default screen of the display 'd'
    int depth = DefaultDepth(d, 0);

    // Allocate and initialize an XImage structure for the display 'd', using the default visual and the determined depth
    // Assuming RGBA
    XImage* image = XCreateImage(d, DefaultVisual(d, 0), depth, ZPixmap, 0, (char*)malloc(p->width * p->height * 4), p->width, p->height, 32, 0);

    // If XCreateImage fails to create the image, return NULL
    if (!image) return NULL;

    // Iterate over each pixel in the PNG_Image to copy its data to the XImage
    for (int y = 0; y < p->height; y++) {
        for (int x = 0; x < p->width; x++) {
            unsigned long pixel = 0; // Temporary storage for the pixel value
            int idx = (y * p->width + x) * 4; // Calculate the index in the PNG data array (4 bytes per pixel)

            // Construct the pixel value for the XImage from the PNG data, assuming BGRX format for the display
            pixel |= p->data[idx + 2]; // Blue
            pixel |= p->data[idx + 1] << 8; // Green
            pixel |= p->data[idx] << 16; // Red

            // The alpha component is not used in XImage.
            // Place the pixel value into the XImage at position (x, y)
            XPutPixel(image, x, y, pixel);
        }
    }

    // Return the pointer to the newly created XImage
    return image;
}

/*
 * sets the given width and height variables to be equal to the current width and height of the window
 */
void get_window_size(Display* display, Window window, int* width, int* height) {
    XWindowAttributes attributes;
    XGetWindowAttributes(display, window, &attributes);

    *width = attributes.width;
    *height = attributes.height;
}

/*
 * Updates the mouse position stored globally
 */
void update_global_mouse_position(){
    Window root, child;
    int rootX, rootY, winX, winY;
    unsigned int mask;

    // Use the specified window instead of DefaultRootWindow
    if (XQueryPointer(d, w, &root, &child, &rootX, &rootY, &winX, &winY, &mask)) {
        // Position relative to the window's top-left corner
        atomic_store(&global_mouse_x, winX);
        atomic_store(&global_mouse_y, winY);
    } // If storage fails here then the global variables will simply be left as they were before
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////// SHUTDOWN FUNCTIONS ///////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * Raises a signal for termination request
 */
void shutdown(){
    raise(SIGTERM);
}

/*
 * Returns the shutdown flag
 */
bool is_gui_shutdown(){
    return atomic_load(&shutdown_flag);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////// WINDOW PARAMETER FUNCTIONS ////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * Struct to contain set_window_x function parameters
 */
typedef struct SetWindowXArgs {
    int set_x;
} SetWindowXArgs;

/*
 * Wrapper function for set_window_x that fits task queue signature requirements
 */
void set_window_x_wrapper(void* arg) {
    SetWindowXArgs* actualArgs = (SetWindowXArgs*) arg;
    set_window_x(actualArgs->set_x);
    free(actualArgs); // Don't forget to free the memory allocated for the args
}

/*
 * Sets the X location attribute for the window
 */
void set_window_x(int set_x){
    if (!in_gui_thread()) {
        // Allocate memory for the arguments struct and set its value
        SetWindowXArgs* args = malloc(sizeof(SetWindowXArgs));
        if (args == NULL) {
            // Handle memory allocation failure
            return;
        }
        args->set_x = set_x;

        // Queue the wrapper function along with the argument struct
        queue_enqueue(&queue, set_window_x_wrapper, args);
    } else {
        // In the GUI thread, execute the function directly
        x = set_x;
    }
}

/*
 * Struct to contain set_window_y function parameters
 */
typedef struct SetWindowYArgs {
    int set_y;
} SetWindowYArgs;

/*
 * Wrapper function for set_window_y that fits task queue signature requirements
 */
void set_window_y_wrapper(void* arg) {
    SetWindowYArgs* actualArgs = (SetWindowYArgs*) arg;
    set_window_y(actualArgs->set_y);
    free(actualArgs); // Don't forget to free the memory allocated for the args
}

/*
 * Sets the Y location attribute for the window
 */
void set_window_y(int set_y){
    if (!in_gui_thread()) {
        // Allocate memory for the arguments struct and set its value
        SetWindowYArgs* args = malloc(sizeof(SetWindowYArgs));
        if (args == NULL) {
            // Handle memory allocation failure
            return;
        }
        args->set_y = set_y;

        // Queue the wrapper function along with the argument struct
        queue_enqueue(&queue, set_window_y_wrapper, args);
    } else {
        // In the GUI thread, execute the function directly
        y = set_y;
    }
}

/*
 * Struct to contain set_window_width function parameters
 */
typedef struct SetWindowWidthArgs {
    int set_width;
} SetWindowWidthArgs;

/*
 * Wrapper function for set_window_width that fits task queue signature requirements
 */
void set_window_width_wrapper(void* arg) {
    SetWindowWidthArgs* actualArgs = (SetWindowWidthArgs*) arg;
    set_window_width(actualArgs->set_width);
    free(actualArgs); // Don't forget to free the memory allocated for the args
}

/*
 * Sets the width location attribute for the window
 */
void set_window_width(int set_width) {
    if (!in_gui_thread()) {
        // Allocate memory for the arguments struct and set its value
        SetWindowWidthArgs* args = malloc(sizeof(SetWindowWidthArgs));
        if (args == NULL) {
            // Handle memory allocation failure
            return;
        }
        args->set_width = set_width;

        // Queue the wrapper function along with the argument struct
        queue_enqueue(&queue, set_window_width_wrapper, args);
    } else {
        // In the GUI thread, execute the function directly
        width = set_width;
    }
}

/*
 * Struct to contain set_window_height function parameters
 */
typedef struct SetWindowHeightArgs {
    int set_height;
} SetWindowHeightArgs;

/*
 * Wrapper function for set_window_height that fits task queue signature requirements
 */
void set_window_height_wrapper(void* arg) {
    SetWindowHeightArgs* actualArgs = (SetWindowHeightArgs*) arg;
    set_window_height(actualArgs->set_height);
    free(actualArgs); // Don't forget to free the memory allocated for the args
}

/*
 * Sets the height location attribute for the window
 */
void set_window_height(int set_height) {
    if (!in_gui_thread()) {
        // Allocate memory for the arguments struct and set its value
        SetWindowHeightArgs* args = malloc(sizeof(SetWindowHeightArgs));
        if (args == NULL) {
            // Handle memory allocation failure
            return;
        }
        args->set_height = set_height;

        // Queue the wrapper function along with the argument struct
        queue_enqueue(&queue, set_window_height_wrapper, args);
    } else {
        // In the GUI thread, execute the function directly
        height = set_height;
    }
}

/*
 * Struct to contain set_window_border_width function parameters
 */
typedef struct SetWindowBorderWidthArgs {
    int set_border_width;
} SetWindowBorderWidthArgs;

/*
 * Wrapper function for set_window_border_width that fits task queue signature requirements
 */
void set_window_border_width_wrapper(void* arg) {
    SetWindowBorderWidthArgs* actualArgs = (SetWindowBorderWidthArgs*) arg;
    set_window_border_width(actualArgs->set_border_width);
    free(actualArgs); // Don't forget to free the memory allocated for the args
}

/*
 * Sets the border width location attribute for the window
 */
void set_window_border_width(int set_border_width) {
    if (!in_gui_thread()) {
        // Allocate memory for the arguments struct and set its value
        SetWindowBorderWidthArgs* args = malloc(sizeof(SetWindowBorderWidthArgs));
        if (args == NULL) {
            // Handle memory allocation failure
            return;
        }
        args->set_border_width = set_border_width;

        // Queue the wrapper function along with the argument struct
        queue_enqueue(&queue, set_window_border_width_wrapper, args);
    } else {
        // In the GUI thread, execute the function directly
        border_width = set_border_width;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////// IMAGE UPDATE FUNCTIONS ///////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * Struct to contain update_image function parameters
 */
typedef struct UpdateImageArgs {
    PNG_Image* image;
} UpdateImageArgs;

/*
 * Wrapper function for update_image that fits task queue signature requirements
 */
void update_image_wrapper(void* arg) {
    UpdateImageArgs* actualArgs = (UpdateImageArgs*) arg;
    update_image(actualArgs->image); // Call the original function with the provided arguments
    free(actualArgs); // Clean up the argument structure
}

/*
 * Updates the image which is displayed in the window
 */
void update_image(PNG_Image* newImage) {
    //TODO: make sure newImage is a copy
    if (!in_gui_thread()) {
        printf("Address of ptrA: %p\n", (void*)newImage);
    
        // Not in the correct thread, enqueue the task
        UpdateImageArgs* args = malloc(sizeof(UpdateImageArgs));
        if (args == NULL) {
            // Handle memory allocation failure
            return;
        }
        // Assume RGBA with 8 bit channel
        args->image = newImage;

        queue_enqueue(&queue, update_image_wrapper, args);
    } else {
        // In the correct thread, execute the update logic directly
        if (newImage != NULL) { // Check that the given image exists
            if (image != NULL) { // Check that the existing image exists
                png_destroy_image(&image); // Destroy the existing image
            }

            // Assume RGBA with 8 bit channel
            image = png_copy_image(newImage); // Set the image to be equal to the new image

            // White background color
            rgba_color background_color = {0xFF, 0xFF, 0xFF, 0xFF};

            // Blend the background of the image to ensure that the image is p
            blend_with_background(image->data, image->width, image->height, background_color);
        }

        // Trigger a redraw
        XEvent event;
        memset(&event, 0, sizeof(event));
        event.type = Expose;
        event.xexpose.window = w;
        event.xexpose.count = 0;
        XSendEvent(d, w, False, ExposureMask, &event);
        XFlush(d);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////// SCALING FUNCTIONS ///////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * Returns true if nearest neighbor scaling is in use, false otherwise
 */
bool get_scaling_nn(){
    return atomic_load(&use_nn);
}

/*
 * Returns true if bilinear interpolation scaling is in use, false otherwise
 */
bool get_scaling_bli(){
    return atomic_load(&use_bli);
}

/*
 * Sets the scaling algorithm to nearest neighbor scaling
 */
void set_scaling_nn(){
    atomic_store(&use_nn, true);
    atomic_store(&use_bli, false);
}

/*
 * Sets the scaling algorithm to nearest bilinear interpolation
 */
void set_scaling_bli(){
    atomic_store(&use_nn, false);
    atomic_store(&use_bli, true);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////// INPUT HANDLING FUNCTIONS ///////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * Struct to contain handle_key_event function parameters
 */
typedef struct HandleKeyEventTaskArgs {
    KeyHandler handler;
    KeySym key;
} HandleKeyEventTaskArgs;

/*
 * Wrapper function for handle_key_event that fits task queue signature requirements
 */
void handle_key_event_wrapper(void* arg) {
    HandleKeyEventTaskArgs* taskArgs = (HandleKeyEventTaskArgs*)arg;
    handle_key_event(taskArgs->handler, taskArgs->key); // Call the original function
    free(taskArgs); // Clean up
}

/*
 * Sets a given KeyHandler (function pointer) to execute when the given key is pressed
 */
void handle_key_event(KeyHandler handler, KeySym key) {
    if (!in_gui_thread()) {
        // Not in the GUI thread, enqueue the task
        HandleKeyEventTaskArgs* args = malloc(sizeof(HandleKeyEventTaskArgs));
        if (args == NULL) {
            // Handle memory allocation failure
            return;
        }
        args->handler = handler;
        args->key = key;

        queue_enqueue(&queue, handle_key_event_wrapper, args);
    } else {
        // Check the start flag before attempting to attach the handler
        if (!atomic_load(&start_flag)) {
            perror("Add Key Handler");
            return;
        }
        
        KeyCode key_code = XKeysymToKeycode(d, key);

        if (key_code < MAX_KEYS) {
            // Directly attach the handler to the given key, assuming this executes on the GUI thread
            key_handlers[key_code] = handler;
        }
    }
}

/*
 * Struct to contain remove_key_event function parameters
 */
typedef struct RemoveKeyHandlerTaskArgs {
    KeySym key;
} RemoveKeyHandlerTaskArgs;

/*
 * Wrapper function for remove_key_event that fits task queue signature requirements
 */
void remove_key_handler_wrapper(void* arg) {
    RemoveKeyHandlerTaskArgs* taskArgs = (RemoveKeyHandlerTaskArgs*)arg;
    remove_key_handler(taskArgs->key); // Invoke the original function
    free(taskArgs); // Free the argument struct
}

/*
 * Removes the event handler for the key specified
 */
void remove_key_handler(KeySym key) {
    if (!in_gui_thread()) {
        // Not in the correct thread, enqueue the task
        RemoveKeyHandlerTaskArgs* args = malloc(sizeof(RemoveKeyHandlerTaskArgs));
        if (args == NULL) {
            // Handle memory allocation failure
            return;
        }
        args->key = key;

        queue_enqueue(&queue, remove_key_handler_wrapper, args);
    } else {
        // In the correct thread, execute the logic directly
        if (!atomic_load(&start_flag)) {
            perror("Remove Key Handler");
            return;
        }
        
        KeyCode key_code = XKeysymToKeycode(d, key);

        if (key_code < MAX_KEYS) {
            // Directly set the handler for the given key to NULL
            key_handlers[key_code] = NULL;
        }
    }
}

/*
 * Modifies the given pointers X and Y to the most recently known position of the mouse
 * Returns 0 if successful or -1 otherwise
 */
int get_mouse_position(int *x, int *y) {
    if (x == NULL || y == NULL) return -1;

    *x = atomic_load(&global_mouse_x);
    *y = atomic_load(&global_mouse_y);

    return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////// GUI STARTING AND LOOP //////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * the main gui loop
 */
void start_window_loop() {
    // Mark the current thread as the GUI thread
    mark_as_gui_thread();

    // Create a simple window with specified dimensions and colors
    w = XCreateSimpleWindow(d, DefaultRootWindow(d), x, y, width, height, border_width, BlackPixel(d, 0), WhitePixel(d, 0));

    // Make the window visible on the screen
    XMapWindow(d, w); // Maps the window on the screen
    // Register interest in certain types of events, including key presses
    XSelectInput(d, w, ExposureMask | StructureNotifyMask | KeyPressMask);

    // Setup to handle window close events
    Atom wmDelete = XInternAtom(d, "WM_DELETE_WINDOW", True);
    XSetWMProtocols(d, w, &wmDelete, 1);

    // Load an image into the global variable 'image' from memory
    image = png_load_from_memory(resources_nagato_png, resources_nagato_png_len);

    // Blend the loaded image with a white background
    rgba_color background_color = {0xFF, 0xFF, 0xFF, 0xFF}; // White
    blend_with_background(image->data, image->width, image->height, background_color);
    

    // Main event loop, continues until 'shutdown_flag' is set
    XEvent event;
    while (!atomic_load(&shutdown_flag)) {
        // Tasks to perform each loop
        process_gui_tasks();
        update_global_mouse_position();

        // Get the next event
        XNextEvent(d, &event);

        // If a window close event is received, initiate application shutdown
        if (event.type == ClientMessage && (Atom)event.xclient.data.l[0] == wmDelete) {
            shutdown();
        } else if (event.type == Expose) {
            // On an expose event, redraw the image
            if (image != NULL) {
                // Dynamically calculate the new size
                get_window_size(d, w, &width, &height);

                // Determine the aspect ratios to decide how to scale the image
                double img_aspect = (double)image->width / image->height;
                double win_aspect = (double)width / height;
                int new_width, new_height;

                // Scale the image based on the aspect ratio comparison
                if (img_aspect > win_aspect) {
                    // Scale based on window width
                    new_width = width;
                    new_height = (int)(width / img_aspect);
                } else {
                    // Scale based on window height
                    new_height = height;
                    new_width = (int)(height * img_aspect);
                }

                // Scale the image
                XImage* scaled_image;
                bool set_default_scaling = false;
                if(use_nn){
                    PNG_Image* temp = nearest_neighbor_scale(image, new_width, new_height);
                    scaled_image = png_image_to_ximage(temp);
                    png_destroy_image(&temp);
                } else if(use_bli){
                    PNG_Image* temp = bilinear_interpolation_scale(image, new_width, new_height);
                    scaled_image = png_image_to_ximage(temp);
                    png_destroy_image(&temp);
                } else {
                    perror("No scaling method set");
                    set_default_scaling = true;
                    PNG_Image* temp = bilinear_interpolation_scale(image, new_width, new_height);
                    scaled_image = png_image_to_ximage(temp);
                    png_destroy_image(&temp);
                }

                // This must be here or there will be a deadlock with aquiring the scaling lock
                if(set_default_scaling) {
                    set_scaling_bli(); // Fallback to bilinear interpolation if no scaling method was set
                }

                // Calculate the position to center the image
                int x_pos = (width - scaled_image->width) / 2;
                int y_pos = (height - scaled_image->height) / 2;

                // Display the scaled image in the window
                XPutImage(d, w, DefaultGC(d, 0), scaled_image, 0, 0, x_pos, y_pos, scaled_image->width, scaled_image->height);

                // Free resources associated with the scaled XImage
                XDestroyImage(scaled_image);
            }
        } else if (event.type == KeyPress) {
            KeySym key = XLookupKeysym(&event.xkey, 0);
            KeyCode key_code = XKeysymToKeycode(d, key);
            if (key_code < MAX_KEYS && key_handlers[key_code]) {
                key_handlers[key_code](); // Execute the key handler if one is set
            }
        }
    }

    // Cleanup
    if (image != NULL) {
        png_destroy_image(&image); // Free memory associated with the image
    }

    queue_destroy(&queue);

    XCloseDisplay(d); // Close connection to X server
}

/*
 * Starts the window loop, with a function signature acceptable for pthread
 */
void* start_gui_thread(void* arg){
    // Wrapper to meet function signature requirements of pthread
    start_window_loop();
    return NULL;
}

/*
 * Starts the gui in its own thread
 */
void start_gui() {
    // If already started do nothing
    if(atomic_load(&start_flag)) return;

    // Init the queue
    queue_init(&queue);

    // Open a connection to the X server
    // Done at this stage to prevent timing issues
    d = XOpenDisplay(NULL);
    if(d == NULL) return; // XOpenDisplay failed, abort

    // Set start_flag
    atomic_store(&start_flag, true);

    // Set up a signal handler for SIGTERM to cleanly terminate the GUI
    signal(SIGTERM, termination_handler);

    // Create a new thread to run the GUI, using start_gui_thread as the start routine
    int err = pthread_create(&thread_id, NULL, start_gui_thread, NULL);
    // Check if the thread was successfully created
    if (err != 0) {
        // If thread creation fails, print an error message and exit
        perror("Failed to create GUI thread");
        exit(err);
    }

    // Detach the GUI thread
    err = pthread_detach(thread_id);
    if (err != 0) {
        perror("Failed to detach GUI thread");
        // Attempt to recover: signal the GUI thread to shutdown
        shutdown();

        // Wait for the GUI thread to finish
        err = pthread_join(thread_id, NULL);
        if (err != 0) {
            perror("Failed to join GUI thread after failed detach");
            exit(err);
        }
    }
}