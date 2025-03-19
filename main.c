#include <stdio.h>
#include <assert.h>
#include <string.h>

#define FBA_IMPLEMENTATION
#define FBA_DEBUG
#include "fixed_buddy_allocator.h"

unsigned char memory[FBA_BLOCK_SIZE * FBA_BLOCK_COUNT];
int main() {
    FixedBuddyAllocator fba = fba_create(memory);
    
    printf("blocks: %08X\n", fba.blocks);
    void* a = fba_allocate(&fba, 88);
    memset(a, 1, 88);
    printf("blocks: %08X\n", fba.blocks);
    void* b = fba_allocate(&fba, 124);
    memset(b, 2, 124);
    printf("blocks: %08X\n", fba.blocks);
    void* c = fba_allocate(&fba, 56);
    memset(c, 4, 56);
    printf("blocks: %08X\n", fba.blocks);
    void* d = fba_allocate(&fba, 104);
    memset(d, 3, 104);
    printf("blocks: %08X\n", fba.blocks);
    
    assert(fba_allocate(&fba, 102) == NULL);
    fba_free(&fba, a, 88);
    printf("blocks: %08X\n", fba.blocks);
    
    fba_allocate(&fba, 8);
    printf("blocks: %08X\n", fba.blocks);
    return 0;
}
