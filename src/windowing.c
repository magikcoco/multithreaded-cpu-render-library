#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <X11/Xatom.h> //Atom handling for close event
#include <X11/keysym.h> //Key handlers
#include <X11/Xlib.h> // X window functions
#include <X11/Xutil.h> // XDestroyImage
#include "logo.h"
#include "png_loading.h"
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

// Array to store handlers for each key
KeyHandler key_handlers[MAX_KEYS];

//image variables
XImage* image;

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

void termination_handler(){
    atomic_store(&shutdown_flag, true);
}

void shutdown(){
    raise(SIGTERM);
}

void set_scaling_nn(){
    pthread_mutex_lock(&scaling_lock);
    use_nn = true;
    use_bli = false;
    pthread_mutex_unlock(&scaling_lock);
}

void set_scaling_bli(){
    pthread_mutex_lock(&scaling_lock);
    use_nn = false;
    use_bli = true;
    pthread_mutex_unlock(&scaling_lock);
}

void set_window_parameters(int set_x, int set_y, int set_width, int set_height, int set_border_width){
    pthread_mutex_lock(&win_param_lock);
    x = set_x;
    y = set_y;
    width = set_width;
    height = set_height;
    border_width = set_border_width;
    pthread_mutex_unlock(&win_param_lock);
}

void get_window_size(Display* display, Window window, int* width, int* height) {
    XWindowAttributes attributes;
    XGetWindowAttributes(display, window, &attributes);

    *width = attributes.width;
    *height = attributes.height;
}

void handle_key_event(KeyHandler handler, KeyMap key) {
    pthread_mutex_lock(&key_handlers_lock);
    if (key < MAX_KEYS) {
        key_handlers[key] = handler;
    }
    pthread_mutex_unlock(&key_handlers_lock);
}

void remove_key_handler(KeyMap key) {
    pthread_mutex_lock(&key_handlers_lock);
    if (key < MAX_KEYS) {
        key_handlers[key] = NULL;
    }
    pthread_mutex_unlock(&key_handlers_lock);
}

void start_window_loop() {
    Display* d = XOpenDisplay(NULL);
    pthread_mutex_lock(&win_param_lock);
    Window w = XCreateSimpleWindow(d, DefaultRootWindow(d), x, y, width, height, border_width, BlackPixel(d, 0), WhitePixel(d, 0));
    pthread_mutex_unlock(&win_param_lock);
    XMapWindow(d, w); // Maps the window on the screen
    XSelectInput(d, w, ExposureMask | StructureNotifyMask | KeyPressMask); // Include KeyPressMask

    // Handle window close event
    Atom wmDelete = XInternAtom(d, "WM_DELETE_WINDOW", True);
    XSetWMProtocols(d, w, &wmDelete, 1);

    //XImage* image = load_png_from_file(d, "resources/nagato.png");
    pthread_mutex_lock(&image_lock);
    image = load_png_from_memory(d, resources_nagato_png, resources_nagato_png_len);
    pthread_mutex_unlock(&image_lock);
    XEvent event;

    // Event loop
    while (!atomic_load(&shutdown_flag)) {
        XNextEvent(d, &event);

        if (event.type == ClientMessage && (Atom)event.xclient.data.l[0] == wmDelete) {
            raise(SIGTERM);
        } else if (event.type == Expose) {
            // Handle Expose event
            if (image != NULL) {
                pthread_mutex_lock(&win_param_lock);
                get_window_size(d, w, &width, &height);

                double img_aspect = (double)image->width / image->height;
                double win_aspect = (double)width / height;

                int new_width, new_height;

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
                pthread_mutex_lock(&scaling_lock);
                pthread_mutex_lock(&image_lock);
                if(use_nn){
                    scaled_image = nearest_neighbor_scale(d, w, image, new_width, new_height);
                } else if(use_bli){
                    scaled_image = bilinear_interpolation_scale(d, w, image, new_width, new_height);
                } else {
                    perror("No scaling method set");
                    set_default_scaling = true;
                    scaled_image = bilinear_interpolation_scale(d, w, image, new_width, new_height);
                }
                pthread_mutex_unlock(&scaling_lock);
                pthread_mutex_unlock(&image_lock);

                //this must be here or there will be a deadlock with aquiring the scaling lock
                if(set_default_scaling) {
                    set_scaling_bli();
                }

                // Calculate the position to center the image
                // From this point the width and the height will be set to their actual value
                int x_pos = (width - scaled_image->width) / 2;
                int y_pos = (height - scaled_image->height) / 2;
                pthread_mutex_unlock(&win_param_lock);

                XPutImage(d, w, DefaultGC(d, 0), scaled_image, 0, 0, x_pos, y_pos, scaled_image->width, scaled_image->height);

                // Destroy the scaled image
                XDestroyImage(scaled_image);
            }
        } else if (event.type == KeyPress) {
            //TODO: worker threads
            KeySym key = XLookupKeysym(&event.xkey, 0);
            pthread_mutex_lock(&key_handlers_lock);
            if (key < MAX_KEYS && key_handlers[key]) {
                key_handlers[key]();  // Call the handler
            }
            pthread_mutex_unlock(&key_handlers_lock);
        }
    }

    // Cleanup
    pthread_mutex_lock(&image_lock);
    if (image != NULL) {
        XDestroyImage(image); // Free memory associated with the image
    }
    pthread_mutex_unlock(&image_lock);

    XCloseDisplay(d); // Close the display
}

void* start_gui_thread(void* arg){
    //wrapper to meet function signature requirements of pthread
    start_window_loop();
    return NULL;
}

void start_gui() {
    XInitThreads();

    signal(SIGTERM, termination_handler);

    int err = pthread_create(&thread_id, NULL, start_gui_thread, NULL);
    if (err != 0) {
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