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

    // Declare an array to hold pointers to PNG_Image structs
    PNG_Image* images[89];

    for (int i = 1; i <= 89; i++) {
        // Allocate memory for the filename
        char filepath[255];

        // Generate the filename. Assuming file names are like frame_apngframe01.png, frame_apngframe02.png, ...
        // Note: Ensure the format specifier %02d to pad with zeroes for numbers less than 10
        sprintf(filepath, "./resources/butterfree/frame_apngframe%02d.png", i);

        // Load the image from the file
        images[i - 1] = png_load_from_file(filepath);

        // Check if the image was successfully loaded
        if (images[i - 1] == NULL) {
            fprintf(stderr, "Failed to load image from %s\n", filepath);
            // Handle error (e.g., break the loop, free allocated images, etc.)
        }
    }

    start_gui();

    set_window_x(0);

    handle_key_event(space_handler, Space);
    handle_key_event(q_handler, K_q);
    handle_key_event(tab_handler, Tab);
    handle_key_event(s_handler, K_s);
    handle_mouse_click(right_click_handler, Mouse_Right_Click);

    int index = 0;

    PNG_Image* flat;
    PNG_Image* bg = png_create_image(1000, 1000, 0xFFFFFF);

    while(!is_gui_shutdown()){
        if(index > 88) {
            index = 0;
        }
        clock_t start = clock();
        for(int j = 0; j < (bg->height / images[index]->height); j++){
            for(int i = 0; i < (bg->width / images[index]->width); i++){
                push_image_raw(images[index], images[index]->width * i, images[index]->height * j);
            }
        }

        push_image_raw(bg, 0, 0);

        if(flat){
            png_destroy_image(&flat);
        }
        flat = get_flattened_image();
        printf("Loop");
        update_image(flat);
        index++;
        frame_rate_control(30);
        clock_t end = clock();

        // Calculate elapsed time in seconds
        float elapsed_time = (float)(end - start) / CLOCKS_PER_SEC;

        // Print the elapsed time
        printf("Total time: %.6f seconds\n", elapsed_time);
    }

    if(flat){
        png_destroy_image(&flat);
    }
    png_destroy_image(&bg);
    png_destroy_image(&logo);
    png_destroy_image(&test);
    png_destroy_image(&color);
    for (int i = 0; i <= 88; i++) {
        png_destroy_image(&images[i]);
    }
}