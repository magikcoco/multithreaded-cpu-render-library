#include "nagato.h"

PNG_Image* test;
PNG_Image* logo;
PNG_Image* color;

void space_handler() {
    update_image(test);
}

void q_handler(){
    update_image(logo);
}

void s_handler(){
    update_image(color);
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
    color = png_create_image(100, 100, 0xFFFFFF);

    start_gui();

    handle_key_event(space_handler, Space);
    handle_key_event(q_handler, K_q);
    handle_key_event(tab_handler, Tab);
    handle_key_event(s_handler, K_s);
    handle_mouse_click(right_click_handler, Mouse_Right_Click);

    while(!is_application_shutdown()){

        clock_t start = clock();

        frame_rate_control(30);
        clock_t end = clock();

        // Calculate elapsed time in seconds
        float elapsed_time = (float)(end - start) / CLOCKS_PER_SEC;

        // Print the elapsed time
        //printf("Total time: %.6f seconds\n", elapsed_time);
    }

    png_destroy_image(&logo);
    png_destroy_image(&test);
    png_destroy_image(&color);
}