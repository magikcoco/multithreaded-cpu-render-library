#include "nagato.h"

void space_handler() {
    update_image_from_file("./resources/test.png");
}

void q_handler(){
    update_image_from_memory(png_load_from_memory(resources_nagato_png, resources_nagato_png_len));
}

void tab_handler(){
    int x = 0;
    int y = 0;
    get_mouse_position(&x, &y);
    printf("(%d, %d)\n", x, y);
}

int main() {
    int x = 50;
    int y = 50;
    int width = 250;
    int height = 250;
    int border_width = 1;
    //Optional, these are the defaults
    set_window_parameters(x, y, width, height, border_width);

    start_gui();

    handle_key_event(space_handler, Space);
    handle_key_event(q_handler, K_q);
    handle_key_event(tab_handler, Tab);

    while(!is_gui_shutdown()){
        frame_rate_control(24);
    }
}