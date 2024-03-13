#include "nagato.h"

PNG_Image* test;
PNG_Image* logo;

void space_handler() {
    update_image(test);
}

void q_handler(){
    update_image(logo);
}

void tab_handler(){
    int x = 0;
    int y = 0;
    int z = get_mouse_position(&x, &y);
    printf("%d (%d, %d)\n", z, x, y);
}

void right_click_handler(){
    printf("Success!\n");
}

int main() {
    test = png_load_from_file("./resources/test.png");
    logo = png_load_from_memory(resources_nagato_png, resources_nagato_png_len);

    start_gui();

    set_window_x(0);

    handle_key_event(space_handler, Space);
    handle_key_event(q_handler, K_q);
    handle_key_event(tab_handler, Tab);
    handle_mouse_click(right_click_handler, Mouse_Right_Click);

    while(!is_gui_shutdown()){
        frame_rate_control(24);
    }

    png_destroy_image(&logo);
    png_destroy_image(&test);
}