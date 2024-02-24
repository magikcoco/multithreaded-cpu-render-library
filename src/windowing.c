#include <stdbool.h>
#include <stdio.h>
#include <X11/Xatom.h> //Atom handling for close event
#include <X11/Xlib.h> // X window functions
#include <X11/Xutil.h> // XDestroyImage
#include "logo.h"
#include "png_loading.h"
#include "scaling.h"
#include "windowing.h"

//TODO: image is updatable, function must trigger expose events
//TODO: event handlers for key presses, other triggers
//TODO: retrieve mouse position

//defaults for window
int x = 50;
int y = 50;
int width = 250;
int height = 250;
int border_width = 1;

//scaling bools
bool use_nn = false;
bool use_bli = false;

void set_scaling_nn(){
    use_nn = true;
    use_bli = false;
}

void set_scaling_bli(){
    use_nn = false;
    use_bli = true;
}

void set_scaling_defaults(){
    set_scaling_bli();
}

void set_window_parameters(int set_x, int set_y, int set_width, int set_height, int set_border_width){
    x = set_x;
    y = set_y;
    width = set_width;
    height = set_height;
    border_width = set_border_width;
}

void get_window_size(Display* display, Window window, int* width, int* height) {
    XWindowAttributes attributes;
    XGetWindowAttributes(display, window, &attributes);

    *width = attributes.width;
    *height = attributes.height;
}

void start_window_loop() {
    Display* d = XOpenDisplay(NULL);
    Window w = XCreateSimpleWindow(d, DefaultRootWindow(d), x, y, width, height, border_width, BlackPixel(d, 0), WhitePixel(d, 0));
    XMapWindow(d, w); // Maps the window on the screen
    XSelectInput(d, w, ExposureMask | StructureNotifyMask);

    // Handle window close event
    Atom wmDelete = XInternAtom(d, "WM_DELETE_WINDOW", True);
    XSetWMProtocols(d, w, &wmDelete, 1);

    //XImage* image = load_png_from_file(d, "resources/nagato.png");
    XImage* image = load_png_from_memory(d, resources_nagato_png, resources_nagato_png_len);
    XEvent event;

    // Event loop
    for (;;) {
        XNextEvent(d, &event);

        if (event.type == ClientMessage && (Atom)event.xclient.data.l[0] == wmDelete) {
            break; // Exit the loop on window close event
        } else if (event.type == Expose) {
            // Handle Expose event
            if (image != NULL) {
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
                if(use_nn){
                    scaled_image = nearest_neighbor_scale(d, w, image, new_width, new_height);
                } else if(use_bli){
                    scaled_image = bilinear_interpolation_scale(d, w, image, new_width, new_height);
                } else {
                    perror("No scaling method set");
                    set_scaling_defaults();
                    scaled_image = bilinear_interpolation_scale(d, w, image, new_width, new_height);
                }

                // Calculate the position to center the image
                // From this point the width and the height will be set to their actual value
                int x_pos = (width - scaled_image->width) / 2;
                int y_pos = (height - scaled_image->height) / 2;

                XPutImage(d, w, DefaultGC(d, 0), scaled_image, 0, 0, x_pos, y_pos, scaled_image->width, scaled_image->height);

                // Destroy the scaled image
                XDestroyImage(scaled_image);
            }
        }
    }

    // Cleanup
    if (image != NULL) {
        XDestroyImage(image); // Free memory associated with the image
    }
    XCloseDisplay(d); // Close the display
}