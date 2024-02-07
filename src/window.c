#include "window.h"
#include <stdio.h>
#include <stdlib.h>

Window create_window(Display *display) {
    Window window;
    int screen;

    // Get the default screen
    screen = DefaultScreen(display);

    // Create a window
    window = XCreateSimpleWindow(display, RootWindow(display, screen), 10, 10, 200, 200, 1,
                                 BlackPixel(display, screen), WhitePixel(display, screen));

    // Select events to monitor
    XSelectInput(display, window, ExposureMask | KeyPressMask);

    // Map the window to the screen
    XMapWindow(display, window);

    return window;
}