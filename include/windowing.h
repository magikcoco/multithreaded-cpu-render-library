#ifndef WINDOWING_H
#define WINDOWING_H

#include <stdbool.h>
#include <X11/Xlib.h> // X window functions
#include "png_image.h"

// Define KeyHandler as a function pointer type
typedef void (*KeyHandler)(void);

// Define KeyCode as an alias for unsigned int
typedef unsigned int KeyMap;

// Shutdown
void shutdown();
bool is_gui_shutdown();

// Window configuration
void set_window_parameters(int set_x, int set_y, int set_width, int set_height, int set_border_width);

// Image update functions
void update_image_from_memory(PNG_Image* newImage);
void update_image_from_file(char *filepath);

// Scaling algorithms
bool get_scaling_nn();
bool get_scaling_bli();
void set_scaling_nn();
void set_scaling_bli();

// Event handling and input query
void handle_key_event(KeyHandler handler, KeySym key);
void remove_key_handler(KeySym key);
int get_mouse_position(int *x, int *y);

// Frame rate control
void frame_rate_control(int targetFPS);

// Start window loop
void start_gui();

#endif // WINDOWING_H
