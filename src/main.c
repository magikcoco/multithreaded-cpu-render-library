#include "nagato.h"

void space_handler() {
    update_image_from_file("./resources/test.png"); 
}

int main() {
    int x = 50;
    int y = 50;
    int width = 250;
    int height = 250;
    int border_width = 1;
    //Optional, these are the defaults
    set_window_parameters(x, y, width, height, border_width);

    handle_key_event(space_handler, Space);

    start_gui();
    return 0;
}