# Description
The goal of this project is to create a GUI library which creates interfaces by compositing pre-made PNG image assets into a single flat image which takes up the whole window, as fast as possible.
# Planned Features and Improvements
- A window which is filled by a single image it is given, or fits the image if it cannot fill
- A compositor which takes an arbitrary number of images and flattens them, to pass to a window
- Mouse location and keypress detection
- Interface to resize the window
- Multithreading on the compositor
- Other optimizations for the compositor