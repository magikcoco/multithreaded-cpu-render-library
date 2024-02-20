#include <png.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "png_loading.h"

int main() {//TODO: move window code to a seperate library
    //TODO: image is always fullscreen or fit to its largest dimension
    //TODO: image is updatable
    //TODO: resizable window
    int x = 50;
    int y = 50;
    int width = 250;
    int height = 250;
    int border_width = 1;
    Display* d = XOpenDisplay(NULL);
    Window w = XCreateSimpleWindow(d, DefaultRootWindow(d), x, y, width, height, border_width, BlackPixel(d, 0), WhitePixel(d, 0));
    XMapWindow(d, w); // Maps the window on the screen
    XSelectInput(d, w, ExposureMask | StructureNotifyMask);

    // Handle window close event
    Atom wmDelete = XInternAtom(d, "WM_DELETE_WINDOW", True);
    XSetWMProtocols(d, w, &wmDelete, 1);

    XImage* image = load_png_from_file(d, "resources/test_image4.png"); // Assume this function exists

    XEvent event;
    // Event loop
    for (;;) {
        XNextEvent(d, &event);

        if (event.type == ClientMessage && (Atom)event.xclient.data.l[0] == wmDelete) {
            break; // Exit the loop on window close event
        } else if (event.type == Expose) {
            // Handle Expose event
            if (image != NULL) {
                XPutImage(d, w, DefaultGC(d, 0), image, 0, 0, 0, 0, image->width, image->height);
            }
        }
    }

    // Cleanup
    if (image != NULL) {
        //FIXME: warnign about implicit declaration
        XDestroyImage(image); // Free memory associated with the image
    }
    XCloseDisplay(d); // Close the display
    return 0;
}