# Description
The goal of this project is to create a GUI library which creates interfaces by compositing pre-made PNG image assets into a single flat image which takes up the whole window, as fast as possible.
# Usage
Currently the project is under development, so the makefile includes flags for Address Sanitizer etc. which affects performance. If you are building this project, I recommend adjusting the makefile before you do.</br></br>
The master header file is nagato.h, which includes compositing.h, key_constants.h, logo.h, png_image.h, scaling.h, and windowing.h. You can include nagato.h in order to use everything.
## compositing
### blend_with_background
Blends an image with a specified background color by modifying the image's pixel data in place. Assumes pixels are represented as four consecutive bytes (RGBA: Red, Green, Blue, Alpha) in a flat array. Sets the opacity to full for every pixel.
## key_constants
### See available key constants below
add an image with a keyboard and a map of each key constant here
## logo
### resources_nagato_png
An array of data representing a PNG image of the logo.
### resources_nagato_png_len
The length of resources_nagato_png.
## png_image
### PNG_Image
A struct for storing PNG image data. It has width, height, bit depth, color type, and data attributes. Bytes per line can be calculated by multiplying the width by the number of channels (in the case of RGBA, width * 4).
### png_load_from_memory
Loads a PNG image from a memory buffer into a custom PNG_Image structure. For example, this function can be used to load the logo. It takes a pointer to memory and the memory size, and returns a pointer to a PNG_Image struct.
### png_load_from_file
Loads a PNG image from a file on disk into a custom PNG_Image structure. It takes a string filepath and returns a pointer to a PNG_Image struct.
### CreatePNG_Image
Creates a PNG_Image struct with the given width, height, bit depth, and color type and returns a pointer to it. The image data is initialized as transparent black.
### DestroyPNG_Image
Safely deallocates all memory used by the given PNG_Image struct.
## scaling
### nearest_neighbor_scale
add documentation here
### bilinear_interpolation_scale
add documentation here
## windowing
### shutdown
Raises a termination signal, which is handled to allow for the graceful shutdown of the GUI thread. This is functionally equivalent to closing the window.
### update_image_from_memory
Given a pointer to a PNG_Image, the display is updated with the new image. The previous image is destroyed during this process using DestroyPNG_Image.
### update_image_from_file
Given a filepath to a file on disk, the image is first loaded into memory using png_load_from_file and then the display is updated with the new image. The previous image is destroyed via DestroyPNG_Image
### get_scaling_nn
Returns true if the scaling algorithm in use by the GUI is nearest neighbor scaling, false otherwise.
### get_scaling_bli
Returns true if the scaling algorithm in use by the GUI is biliniear interpolation scaling, false otherwise.
### set_scaling_nn
Sets the GUI to use nearest neighbor scaling. This is the fastest scaling algorithm, however typically results in a pixelated image or otherwise causes some clearly visible artifacting. Best used on images with very hard edges.
### set_scaling_bli
Returns true if the scaling algorithm in use by the GUI is biliniear interpolation scaling, false otherwise. When no algorithm is set, the GUI will fall back on this algorithm. It is better at handling gradual gradients than nearest neighbor, but may result in blurry images and is not ideal when sharp details are a priority.
### handle_key_event
Adds the given key handler to the given key. Whenever that key is pressed, the key handler will trigger immediately.
### remove_key_handler
Removes the key handler for a particular key. After calling this function, nothing will happen when pressing that key.
### set_window_parameters
Sets the coordinates on the screen, the dimensions, and the border width of the window.
### start_gui
Starts the GUI loop in the GUI thread. Blocks the current thread.
# Planned Features and Improvements
- Mouse tracking
- Frame rate controls
- Image stack compositing/flattening
- Expanding multithreading capabilities
- Bicubic interpolation scaling
- Lanczos resampling scaling
- Box sampling
- Memory and CPU optimizations