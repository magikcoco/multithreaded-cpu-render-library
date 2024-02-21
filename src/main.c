#include "png_loading.h"
#include "windowing.h"

int main() {
    int x = 50;
    int y = 50;
    int width = 250;
    int height = 250;
    int border_width = 1;
    start_window_loop(x, y, width, height, border_width);
    return 0;
}