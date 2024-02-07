#include "my_gui.h"
#include "renderer.h"
#include "window.h"
#include <stdio.h>
#include <unistd.h>  // For sleep()
#include <stdlib.h>
#include <X11/Xlib.h>

int main() {
    printf("Test Program Starting...\n");

    Display *display;
    Window window;
    XEvent event;

    // Open a connection to the X server
    display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "Unable to open display\n");
        exit(1);
    }

    // Create a window
    window = create_window(display);

    // Event loop
    while (1) {
        XNextEvent(display, &event);
        switch (event.type) {
            case Expose:
                // Handle expose event
                break;
            case KeyPress:
                // Handle key press event
                XCloseDisplay(display);
                return 0;
            default:
                break;
        }
    }

    return 0;
}
