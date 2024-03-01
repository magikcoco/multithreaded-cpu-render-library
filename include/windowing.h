#ifndef WINDOWING_H
#define WINDOWING_H

#include <stdbool.h>
#include "png_image.h"

// Define KeyHandler as a function pointer type
typedef void (*KeyHandler)(void);

// Define KeyCode as an alias for unsigned int
typedef unsigned int KeyMap;

/*
 * raises a signal for termination request
 */
void shutdown();

/*
 * updates the image displayed in the window using an existing PNG_Image in memory
 */
void update_image_from_memory(PNG_Image* newImage);

/*
 * updates the image displayed in the window using a filepath
 */
void update_image_from_file(char *filepath);

/*
 * returns true if nearest neighbor scaling is in use, false otherwise
 */
bool get_scaling_nn();

/*
 * returns true if bilinear interpolation scaling is in use, false otherwise
 */
bool get_scaling_bli();

/*
 * sets the scaling algorithm to nearest neighbor scaling
 */
void set_scaling_nn();

/*
 * sets the scaling algorithm to nearest bilinear interpolation
 */
void set_scaling_bli();

/*
 * adds a key event handler
 */
void handle_key_event(KeyHandler handler, KeyMap key);

/*
 * removes the event handler for the key specified
 */
void remove_key_handler(KeyMap key);

/*
 * sets the window parameters
 */
void set_window_parameters(int set_x, int set_y, int set_width, int set_height, int set_border_width);

/*
 * starts the window loop, with a function signature acceptable for pthread
 */
void start_gui();

#endif // WINDOWING_H
