#ifndef WINDOWING_H
#define WINDOWING_H

#include "key_constants.h" // Custom key constants

// Define KeyHandler as a function pointer type
typedef void (*KeyHandler)(void);

// Define KeyCode as an alias for unsigned int
typedef unsigned int KeyMap;

// Function declaration
void shutdown();
void set_scaling_nn();
void set_scaling_bli();
void handle_key_event(KeyHandler handler, KeyMap key);
void remove_key_handler(KeyMap key);
void set_window_parameters(int set_x, int set_y, int set_width, int set_height, int set_border_width);
void start_gui();

#endif // WINDOWING_H
