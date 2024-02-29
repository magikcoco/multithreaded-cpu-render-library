#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <X11/Xatom.h> //Atom handling for close event
#include <X11/keysym.h> //Key handlers
#include <X11/Xlib.h> // X window functions
#include <X11/Xutil.h> // XDestroyImage
#include "logo.h"
#include "compositing.h"
#include "png_image.h"
#include "scaling.h"
#include "windowing.h"

//TODO: change window parameters while gui is operating
//TODO: image is updatable, function must trigger expose events
//TODO: retrieve mouse position

#define MAX_KEYS 256

//defaults for window
int x = 50;
int y = 50;
int width = 250;
int height = 250;
int border_width = 1;

//other globals
Display* d;
Window w;

// Array to store handlers for each key
KeyHandler key_handlers[MAX_KEYS];

//image variables
//XImage* image;
PNG_Image* image;

//scaling bools
bool use_nn = false;
bool use_bli = false;

//shutdown bool
atomic_bool shutdown_flag = false;

//multithreading variables
pthread_t thread_id;
pthread_mutex_t scaling_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t win_param_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t image_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t key_handlers_lock = PTHREAD_MUTEX_INITIALIZER;

/*
 * handles what to do when SIGTERM is received
 */
void termination_handler(){
    atomic_store(&shutdown_flag, true);
}

/*
 * raises a signal for termination request
 */
void shutdown(){
    raise(SIGTERM);
}
//TODO: update image from memory

/*
 * updates the image displayed in the window using a filepath
 */
void update_image_from_file(char *filepath){
    // Lock a mutex before updating the global image to prevent concurrent access
    pthread_mutex_lock(&image_lock);

    // Load a new image from the specified file path
    image = png_load_from_file(filepath);

    //TODO: make this global and changable
    // Define a white background color.
    rgba_color background_color = {0xFF, 0xFF, 0xFF, 0xFF}; // White
    // Blend the loaded image with the white background color
    blend_with_background(image->data, image->width, image->height, background_color);

    // Unlock the mutex after the image has been updated
    pthread_mutex_unlock(&image_lock);

    // Initialize an XEvent structure for triggering a redraw
    XEvent event;
    memset(&event, 0, sizeof(event)); // Clear the event structure to zero
    event.type = Expose; // Set the event type to Expose, indicating a redraw is needed
    event.xexpose.window = w; // Specify the window to be redrawn
    event.xexpose.count = 0; // Set to 0 to indicate this is the last expose event

    // Send the Expose event to the specified window to trigger a redraw
    XSendEvent(d, w, False, ExposureMask, &event);

    // Flush the output buffer to ensure the X server processes all pending requests
    XFlush(d);
}

/*
 * returns true if nearest neighbor scaling is in use, false otherwise
 */
bool get_scaling_nn(){
    bool ret_bool = false;
    pthread_mutex_lock(&scaling_lock);
    if(use_nn) ret_bool = true;
    pthread_mutex_unlock(&scaling_lock);
    return ret_bool;
}

/*
 * returns true if bilinear interpolation scaling is in use, false otherwise
 */
bool get_scaling_bli(){
    bool ret_bool = false;
    pthread_mutex_lock(&scaling_lock);
    if(use_bli) ret_bool = true;
    pthread_mutex_unlock(&scaling_lock);
    return ret_bool;
}

/*
 * sets the scaling algorithm to nearest neighbor scaling
 */
void set_scaling_nn(){
    pthread_mutex_lock(&scaling_lock);
    use_nn = true;
    use_bli = false;
    pthread_mutex_unlock(&scaling_lock);
}

/*
 * sets the scaling algorithm to nearest bilinear interpolation
 */
void set_scaling_bli(){
    pthread_mutex_lock(&scaling_lock);
    use_nn = false;
    use_bli = true;
    pthread_mutex_unlock(&scaling_lock);
}

//TODO: break this up into individual getters and setters

/*
 * sets the window parameters
 */
void set_window_parameters(int set_x, int set_y, int set_width, int set_height, int set_border_width){
    pthread_mutex_lock(&win_param_lock);
    x = set_x;
    y = set_y;
    width = set_width;
    height = set_height;
    border_width = set_border_width;
    pthread_mutex_unlock(&win_param_lock);
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
 * adds a key event handler
 */
void handle_key_event(KeyHandler handler, KeyMap key) {
    //acquire the key handlers lock
    pthread_mutex_lock(&key_handlers_lock);
    if (key < MAX_KEYS) {
        //attach the handler to the given key
        key_handlers[key] = handler;
    }
    pthread_mutex_unlock(&key_handlers_lock);
}

/*
 * removes the event handler for the key specified
 */
void remove_key_handler(KeyMap key) {
    //acquire the key handlers lock
    pthread_mutex_lock(&key_handlers_lock);
    if (key < MAX_KEYS) {
        //change the given key to NULL
        key_handlers[key] = NULL;
    }
    pthread_mutex_unlock(&key_handlers_lock);
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

void start_window_loop() {
    // Open a connection to the X server
    d = XOpenDisplay(NULL);

    // Lock the mutex to safely update window parameters
    pthread_mutex_lock(&win_param_lock);
    // Create a simple window with specified dimensions and colors
    w = XCreateSimpleWindow(d, DefaultRootWindow(d), x, y, width, height, border_width, BlackPixel(d, 0), WhitePixel(d, 0));
    // Unlock the mutex after updating
    pthread_mutex_unlock(&win_param_lock);

    // Make the window visible on the screen
    XMapWindow(d, w); // Maps the window on the screen
    // Register interest in certain types of events, including key presses
    XSelectInput(d, w, ExposureMask | StructureNotifyMask | KeyPressMask);

    // Setup to handle window close events
    Atom wmDelete = XInternAtom(d, "WM_DELETE_WINDOW", True);
    XSetWMProtocols(d, w, &wmDelete, 1);

    // Lock the mutex to safely update the global image
    pthread_mutex_lock(&image_lock);
    // Load an image into the global variable 'image' from memory
    image = png_load_from_memory(resources_nagato_png, resources_nagato_png_len);
    // Blend the loaded image with a white background
    rgba_color background_color = {0xFF, 0xFF, 0xFF, 0xFF}; // White
    blend_with_background(image->data, image->width, image->height, background_color);
    // Unlock the mutex after the update
    pthread_mutex_unlock(&image_lock);
    

    // Main event loop, continues until 'shutdown_flag' is set
    XEvent event;
    while (!atomic_load(&shutdown_flag)) {
        XNextEvent(d, &event);

        // If a window close event is received, initiate application shutdown
        if (event.type == ClientMessage && (Atom)event.xclient.data.l[0] == wmDelete) {
            raise(SIGTERM);
        } else if (event.type == Expose) {
            // On an expose event, redraw the image
            if (image != NULL) {
                //TODO: optimze lock acquisition and release

                // Dynamically calculate the new size
                pthread_mutex_lock(&win_param_lock);
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
                pthread_mutex_lock(&scaling_lock); //get the scaling lock
                pthread_mutex_lock(&image_lock); //get the image lock
                if(use_nn){
                    scaled_image = png_image_to_ximage(nearest_neighbor_scale(image, new_width, new_height));
                } else if(use_bli){
                    scaled_image = png_image_to_ximage(bilinear_interpolation_scale(image, new_width, new_height));
                } else {
                    perror("No scaling method set");
                    set_default_scaling = true;
                    //scaled_image = bilinear_interpolation_scale(d, w, image, new_width, new_height);
                    scaled_image = png_image_to_ximage(bilinear_interpolation_scale(image, new_width, new_height));
                }
                pthread_mutex_unlock(&scaling_lock); //release scaling lock
                pthread_mutex_unlock(&image_lock); //release image lock

                // This must be here or there will be a deadlock with aquiring the scaling lock
                if(set_default_scaling) {
                    set_scaling_bli(); // Fallback to bilinear interpolation if no scaling method was set
                }

                // Calculate the position to center the image
                int x_pos = (width - scaled_image->width) / 2;
                int y_pos = (height - scaled_image->height) / 2;
                pthread_mutex_unlock(&win_param_lock); // Release window parameter lock

                // Display the scaled image in the window
                XPutImage(d, w, DefaultGC(d, 0), scaled_image, 0, 0, x_pos, y_pos, scaled_image->width, scaled_image->height);

                // Free resources associated with the scaled XImage
                XDestroyImage(scaled_image);
            }
        } else if (event.type == KeyPress) {
            //TODO: worker threads
            KeySym key = XLookupKeysym(&event.xkey, 0);
            pthread_mutex_lock(&key_handlers_lock);
            if (key < MAX_KEYS && key_handlers[key]) {
                key_handlers[key](); // Execute the key handler if one is set
            }
            pthread_mutex_unlock(&key_handlers_lock);
        }
    }

    // Cleanup
    pthread_mutex_lock(&image_lock);
    if (image != NULL) {
        DestroyPNG_Image(&image); // Free memory associated with the image
    }
    pthread_mutex_unlock(&image_lock);

    XCloseDisplay(d); // Close the display
}

/*
 * starts the window loop, with a function signature acceptable for pthread
 */
void* start_gui_thread(void* arg){
    //wrapper to meet function signature requirements of pthread
    start_window_loop();
    return NULL;
}

/*
 * starts the gui in its own thread
 */
void start_gui() {
    // Initialize X11 library for multithreading support
    XInitThreads();

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
    /*
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
    */
    //TODO: switch to the above when there is something else for the main thread to do
    err = pthread_join(thread_id, NULL);
    if (err != 0) {
        perror("Failed to join GUI thread after failed detach");
        exit(err);
    }
}