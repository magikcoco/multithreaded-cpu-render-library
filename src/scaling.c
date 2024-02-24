#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h> // XGetPixel, XPutPixel

//TODO: Bicubic interpolation
//TODO: Lanczos Resampling
//TODO: Box Sampling

unsigned int getR(unsigned int color) {
    return (color & 0x00FF0000) >> 16;
}

unsigned int getG(unsigned int color) {
    return (color & 0x0000FF00) >> 8;
}

unsigned int getB(unsigned int color) {
    return (color & 0x000000FF);
}

XImage* nearest_neighbor_scale(Display* display, Window window, XImage* orig, int new_width, int new_height) {
    int x_ratio = (int)((orig->width << 16) / new_width) + 1;
    int y_ratio = (int)((orig->height << 16) / new_height) + 1;

    Visual *visual = DefaultVisual(display, 0);
    XImage* scaled = XCreateImage(display, visual, orig->depth, ZPixmap, 0, NULL, new_width, new_height, 32, 0);
    scaled->data = (char *) malloc(scaled->bytes_per_line * scaled->height);

    if (!scaled || !scaled->data) {
        perror("Failed to create scaled image");
        if (scaled) XDestroyImage(scaled);
        return orig;
    }

    if (scaled->data == NULL) {
        perror("Failed to allocate memory for scaled image data");
        XDestroyImage(scaled);
        return orig;
    }

    for (int i = 0; i < new_height; i++) {
        for (int j = 0; j < new_width; j++) {
            int x2 = ((j * x_ratio) >> 16);
            int y2 = ((i * y_ratio) >> 16);

            // Boundary checks
            x2 = (x2 >= orig->width) ? orig->width - 1 : x2;
            y2 = (y2 >= orig->height) ? orig->height - 1 : y2;

            unsigned int orig_pixel = *((unsigned int*)(orig->data + y2 * orig->bytes_per_line + x2 * 4));
            *((unsigned int*)(scaled->data + i * scaled->bytes_per_line + j * 4)) = orig_pixel;
        }
    }

    return scaled;
}

XImage* bilinear_interpolation_scale(Display* display, Window window, XImage* orig, int new_width, int new_height) {
    Visual *visual = DefaultVisual(display, 0);
    XImage *scaled = XCreateImage(display, visual, orig->depth, orig->format, 0, NULL, new_width, new_height, 32, 0);
    scaled->data = (char *) malloc(scaled->bytes_per_line * scaled->height);

    if (scaled->data == NULL) {
        perror("Bilinear interpolation");
        return orig;
    }

    double x_ratio = (double)(orig->width - 1) / new_width;
    double y_ratio = (double)(orig->height - 1) / new_height;
    double px, py;

    for (int i = 0; i < new_height; i++) {
        for (int j = 0; j < new_width; j++) {
            px = floor(j * x_ratio);
            py = floor(i * y_ratio);
            double fx = j * x_ratio - px;
            double fy = i * y_ratio - py;

            // Boundary checks
            int px1 = (px + 1 < orig->width) ? px + 1 : px;
            int py1 = (py + 1 < orig->height) ? py + 1 : py;

            int p1 = *((unsigned int*)(orig->data + (int)py * orig->bytes_per_line + (int)px * 4));
            int p2 = *((unsigned int*)(orig->data + (int)py * orig->bytes_per_line + (int)px1 * 4));
            int p3 = *((unsigned int*)(orig->data + (int)py1 * orig->bytes_per_line + (int)px * 4));
            int p4 = *((unsigned int*)(orig->data + (int)py1 * orig->bytes_per_line + (int)px1 * 4));

            unsigned int r = (unsigned int)(getR(p1) * (1 - fx) * (1 - fy) + getR(p2) * fx * (1 - fy) + getR(p3) * (1 - fx) * fy + getR(p4) * fx * fy);
            unsigned int g = (unsigned int)(getG(p1) * (1 - fx) * (1 - fy) + getG(p2) * fx * (1 - fy) + getG(p3) * (1 - fx) * fy + getG(p4) * fx * fy);
            unsigned int b = (unsigned int)(getB(p1) * (1 - fx) * (1 - fy) + getB(p2) * fx * (1 - fy) + getB(p3) * (1 - fx) * fy + getB(p4) * fx * fy);

            *((unsigned int*)(scaled->data + i * scaled->bytes_per_line + j * 4)) = (r << 16) | (g << 8) | b;
        }
    }

    return scaled;
}