#ifndef WINDOWING_H
#define WINDOWING_H

#include <stdbool.h>

// Define KeyHandler as a function pointer type
typedef void (*KeyHandler)(void);

// Define KeyCode as an alias for unsigned int
typedef unsigned int KeyMap;

// Function declaration
void shutdown();
void update_image_from_file(char *filepath);
bool get_scaling_nn();
bool get_scaling_bli();
void set_scaling_nn();
void set_scaling_bli();
void handle_key_event(KeyHandler handler, KeyMap key);
void remove_key_handler(KeyMap key);
void set_window_parameters(int set_x, int set_y, int set_width, int set_height, int set_border_width);
void start_gui();

#endif // WINDOWING_H
