#include <X11/Xlib.h>

int main() {
    XEvent event;
    Display* d = XOpenDisplay(NULL);
    int x = 50;
    int y = 50;
    int width = 250;
    int height = 250;
    int border_width = 1;
    Window w = XCreateSimpleWindow(d, DefaultRootWindow(d), x, y, width, height, border_width, BlackPixel(d, 0), WhitePixel(d, 0));
    XMapWindow(d, w); // Maps the window on the screen
    XSelectInput(d, w, ExposureMask);

    // Event loop
    for(;;){
        XNextEvent(d, &event);
        if(event.type == Expose){
            XDrawString(d, w, DefaultGC(d, 0), 100, 100, "Hello World!", 12);
        }
    }
    return 0;
}