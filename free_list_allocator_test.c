#include <stdio.h>

#define FLA_IMPLEMENTATION
#include "free_list_allocator.h"


int main() {
    printf("hello world\n");


    FreeListAllocator fla = fla_create(1024 * 1024);
    fla_dump_nodes(&fla);
    printf("------\n");

    void* ptr = fla_allocate(&fla, 20);
    fla_dump_nodes(&fla);
    printf("------\n");

    fla_free(&fla, ptr);
    fla_dump_nodes(&fla);
    printf("------\n");
    
    size_t count = 16;
    void** array = fla_allocate(&fla, count * sizeof(void*));
    fla_dump_nodes(&fla);
    printf("------\n");

    for (size_t i = 0; i < count; i++) {
        array[i] = fla_allocate(&fla, i * 16 + 8);
    }
    fla_dump_nodes(&fla);
    printf("------\n");

    for (size_t i = 1; i < count; i += 2) {
        fla_free(&fla, array[i]);
    }
    fla_dump_nodes(&fla);
    printf("------\n");

    for (size_t i = 1; i < count; i+=4) {
        array[i] = fla_allocate(&fla, i * 16 + 8);
    }
    fla_dump_nodes(&fla);
    printf("------\n");

    for (size_t i = 0; i < count; i += 2) {
        fla_free(&fla, array[i]);
    }
    fla_dump_nodes(&fla);
    printf("------\n");
    for (size_t i = 1; i < count; i += 4) {
        fla_free(&fla, array[i]);
    }
    fla_dump_nodes(&fla);
    printf("------\n");

    fla_free(&fla, array);
    fla_dump_nodes(&fla);


    return 0;
}
