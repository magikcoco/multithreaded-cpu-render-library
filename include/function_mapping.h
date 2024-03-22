#ifndef FUNCTION_MAPPING_H
#define FUNCTION_MAPPING_H

// Function pointer type definition for generic functions
typedef void (*FunctionPointer)(void *);

typedef struct {
    FunctionPointer ptr;
    const char* name;
} FunctionMapEntry;

/*
 * Add a function to the map
 */ 
void register_function(FunctionPointer ptr, const char* name);

/*
 * Get a function's name by its pointer
 */
const char* get_function_name(FunctionPointer ptr);

#endif // FUNCTION_MAPPING_H
