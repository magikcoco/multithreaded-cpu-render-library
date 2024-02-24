#include "png_loading.h"
#include "windowing.h"

int main() {
    int x = 50;
    int y = 50;
    int width = 250;
    int height = 250;
    int border_width = 1;
    //Optional, these are the defaults
    set_window_parameters(x, y, width, height, border_width);
    start_window_loop();
    return 0;
}