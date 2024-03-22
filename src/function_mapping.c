#include <stdio.h>
#include "function_mapping.h"

// Define a reasonable size for your function map.
#define MAX_FUNCTIONS 100

static FunctionMapEntry function_map[MAX_FUNCTIONS];
static size_t function_count = 0;

void register_function(FunctionPointer ptr, const char* name) {
    if (function_count < MAX_FUNCTIONS) {
        function_map[function_count].ptr = ptr;
        function_map[function_count].name = name;
        ++function_count;
    } else {
        // TODO: Implement resizing
        perror("function map is full");
    }
}

const char* get_function_name(FunctionPointer ptr) {
    for (size_t i = 0; i < function_count; ++i) {
        if (function_map[i].ptr == ptr) {
            return function_map[i].name;
        }
    }
    return "Unknown function";
}