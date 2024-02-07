#include "my_gui.h"
#include "renderer.h"
#include <stdio.h>

int main() {
    init_gui();
    render();
    start_gui(); // Note: This will enter an infinite loop as per your current implementation
    printf("Finished\n");
    return 0;
}
