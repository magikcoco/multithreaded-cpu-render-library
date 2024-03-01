#include "nagato.h"

void space_handler() {
    update_image_from_file("./resources/test.png"); 
}

void q_handler(){
    update_image_from_memory(png_load_from_memory(resources_nagato_png, resources_nagato_png_len));
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
    handle_key_event(q_handler, K_q);

    start_gui();
    return 0;
}