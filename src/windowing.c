#include <X11/Xatom.h> //Atom handling for close event
#include <X11/Xlib.h> // X window functions
#include "logo.h"
#include "png_loading.h"
#include "windowing.h"

void start_window_loop(int x, int y, int width, int height, int border_width) {
    //TODO: image is always fullscreen or fit to its largest dimension
    //TODO: image is updatable
    //TODO: resizable window
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
                XPutImage(d, w, DefaultGC(d, 0), image, 0, 0, 0, 0, image->width, image->height);
            }
        }
    }

    // Cleanup
    if (image != NULL) {
        //FIXME: warning about implicit declaration
        XDestroyImage(image); // Free memory associated with the image
    }
    XCloseDisplay(d); // Close the display
}