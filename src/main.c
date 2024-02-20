#include <png.h>
#include <X11/Xlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

bool initialize_png_reader(const char *filepath, FILE **fp, png_structp *png_ptr, png_infop *info_ptr) {
    // Open the file
    *fp = fopen(filepath, "rb");

    // Check if the file was successfully opened
    if (!*fp) {
        perror("Error opening file");
        return false;
    }

    // Initialize libpng read structure
    *png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    // Check if the read struct was created successfully
    if (!*png_ptr) {
        perror("Error creating png read struct");
        fclose(*fp);
        return false;
    }

    // Create PNG info struct
    *info_ptr = png_create_info_struct(*png_ptr);

    // Check if the PNG info struct was created successfully
    if (!*info_ptr) {
        perror("Error creating png info struct");
        png_destroy_read_struct(png_ptr, NULL, NULL);
        fclose(*fp);
        return false;
    }

    // Setup setjmp for handling libpng errors, if an error occurs in a libpng function after this it will jump back here
    if (setjmp(png_jmpbuf(*png_ptr))) {
        perror("Error during png init_io");
        png_destroy_read_struct(png_ptr, info_ptr, NULL);
        fclose(*fp);
        return false;
    }

    // Initializes the I/O for the libpng file reading process using the previously opened file.
    png_init_io(*png_ptr, *fp);
    return true;
}

void apply_color_transformations(png_structp png, png_infop info) {
    //FIXME: some images have a blue hue
    //Check that color type and bit depth handling are done correctly
    //Check that all color types are handled correctly
    //Check the alpha channel handling
    //Byte order (RGBA vs BGRA) could be causing the blue tint
    //Add handling for gamma correction
    //Check memory allocation
    png_byte color_type = png_get_color_type(png, info);
    png_byte bit_depth = png_get_bit_depth(png, info);

    if (bit_depth == 16) {
        png_set_strip_16(png);
    }

    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png);
    }

    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
        png_set_expand_gray_1_2_4_to_8(png);
    }

    if (png_get_valid(png, info, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png);
    }

    if (color_type == PNG_COLOR_TYPE_RGB ||
        color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    }

    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png);
    }
}

png_bytep *read_png_data(png_structp png, png_infop info, int height) {
    png_bytep *row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height);
    for(int y = 0; y < height; y++) {
        row_pointers[y] = (png_byte*)malloc(png_get_rowbytes(png, info));
    }

    png_read_image(png, row_pointers);
    return row_pointers;
}

png_bytep create_contiguous_image_data(png_structp png, png_infop info, png_bytep *row_pointers, int height) {
    // Allocate memory for the entire image
    png_bytep img_data = (png_bytep)malloc(height * png_get_rowbytes(png, info));

    for (int y = 0; y < height; y++) {
        memcpy(img_data + y * png_get_rowbytes(png, info), row_pointers[y], png_get_rowbytes(png, info));
        free(row_pointers[y]);
    }

    return img_data;
}

XImage *load_png_from_file(Display *display, char *filepath) {
    //TODO: refactor more
    FILE *fp;
    png_structp png;
    png_infop info;

    if (!initialize_png_reader(filepath, &fp, &png, &info)) {
        return NULL; // Initialization failed, return early
    }

    png_read_info(png, info);
    
    apply_color_transformations(png, info);

    png_read_update_info(png, info);

    int height = png_get_image_height(png, info);
    png_bytep *row_pointers = read_png_data(png, info, height);

    fclose(fp);

    png_bytep img_data = create_contiguous_image_data(png, info, row_pointers, height);

    free(row_pointers); // Free the row pointers array

    // Create the XImage using the contiguous block
    Visual *visual = DefaultVisual(display, 0);
    XImage *img = XCreateImage(display, visual, DefaultDepth(display, 0), ZPixmap, 0, (char*)img_data, png_get_image_width(png, info), height, 32, 0);

    return img;
}

int main() {
    //TODO: move window code to a seperate library
    //TODO: resizable image
    //TODO: image is always fullscreen or fit to its largest dimension

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

    // Handle window close event
    Atom wmDelete = XInternAtom(d, "WM_DELETE_WINDOW", True);
    XSetWMProtocols(d, w, &wmDelete, 1);

    // Load the PNG image
    XImage* image = load_png_from_file(d, "resources/test_image.png");

    // Event loop
    for(;;){
        XNextEvent(d, &event); // Stores the enxt event from the X server in event

        if(event.type == ClientMessage && (Atom)event.xclient.data.l[0] == wmDelete) {
            // Delete window event
            break; // Exit the loop
        } else if(event.type == Expose){
            // Expose event, requires the window to be redrawn
            if(image != NULL) {
                int src_x = 0;
                int src_y = 0;
                int dest_x = 0;
                int dest_y = 0;
                XPutImage(d, w, DefaultGC(d, 0), image, src_x, src_y, dest_x, dest_y, image->width, image->height);
            }
        }
    }

    // Cleanup and exit
    if(image != NULL) {
        //FIXME: throws an implicit declaration of function warning
        XDestroyImage(image); // Free memory asscociated with the image
    }
    XCloseDisplay(d); // Close Display

    return 0;
}