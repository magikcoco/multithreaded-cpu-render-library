#include <stdio.h>
#include "key_constants.h" // Custom key constants
#include "png_loading.h"
#include "windowing.h"

void example_handler() {
    printf("test\n");
}

int main() {
    int x = 50;
    int y = 50;
    int width = 250;
    int height = 250;
    int border_width = 1;
    //Optional, these are the defaults
    set_window_parameters(x, y, width, height, border_width);

    handle_key_event(example_handler, Space);

    start_gui();
    return 0;
}