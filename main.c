#include <stdio.h>
#include <limits.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>

#define MEMORY_DEBUG
#define UNUSED_MEMORY_VALUE 0xCD
#define BLOCK_SIZE 16
typedef unsigned int blocks_t;
#define BLOCK_COUNT (sizeof(blocks_t) * CHAR_BIT)
#define BLOCKS_ONES ((blocks_t)-1)

unsigned char memory[BLOCK_SIZE * BLOCK_COUNT];

size_t ceil_divide(size_t a, size_t b) {
    return (a + b - 1) / b;
}

unsigned int count_leading_zeros(unsigned int num) {
    unsigned int bit_count = sizeof(num) * CHAR_BIT;
    unsigned int center = bit_count / 2;
    while (center != 0) {
        unsigned int higher_half = num >> center;
        if (higher_half != 0) {
            bit_count = bit_count - center;
            num = higher_half;
        }
        center = center >> 1;
    }
    return bit_count - num;
}

unsigned int count_trailing_zeros(unsigned int num) {
    unsigned int count = 32;
    num &= -num;
    if (num) {
        count--;
    }
    if (num & 0x0000FFFF) {
        count -= 16;
    }
    if (num & 0x00FF00FF) {
        count -= 8;
    }
    if (num & 0x0F0F0F0F) {
        count -= 4;
    }
    if (num & 0x33333333) {
        count -= 2;
    }
    if (num & 0x55555555) { 
        count -= 1;
    }
    return count;
}
unsigned int used_bits(unsigned int num) {
    return sizeof(unsigned int) * CHAR_BIT - count_leading_zeros(num) - 1;
}

size_t round_up_to_next_power_of_two(size_t num) {
    num--;
    num |= num >> 1;
    num |= num >> 2;
    num |= num >> 4;
    num |= num >> 8;
    num |= num >> 16;
    num++;
    return num;
}
size_t find_optimal_space(blocks_t blocks, size_t levels) {
    assert(levels < 5);
    const blocks_t patterns[4] = {
        0x55555555,
        0x33333333,
        0x0F0F0F0F,
        0x00FF00FF
    };

    blocks_t free = blocks;
    for (size_t i = 0; i < levels; i++) {
        blocks_t level = 1 << i;
        // finding any pairs which are all '1' based on the previous level
        free = free & (free >> level) & patterns[i];
        // filling the entire level with 1
        free |= free << level;
    }
    blocks_t where = free;

    for (size_t i = levels; i < 4; i++) {
        blocks_t level = 1 << i;
        // finding any pairs which are all '1' based on the previous level
        free = free & (free >> level) & patterns[i];
        // filling the entire level with 1
        free |= free << level;
        // getting the occupied areas based on the level - 1 occupied, 0 free
        blocks_t occupied = ~free;

        // filtering the potential places to be only in the partially occupied area
        blocks_t new_where = where & occupied;

        // checking if the partially occupied area has place for the new size
        if (new_where != 0) {
            where = new_where;
        }
    }
    if (where == 0) {
        return (size_t)-1;
    }
    else {
        return count_trailing_zeros(where);
    }
}



void* allocate(blocks_t* blocks, size_t size) {
    size_t min_blocks_required = ceil_divide(size, BLOCK_SIZE);
    size_t blocks_required = round_up_to_next_power_of_two(min_blocks_required);
    size_t levels = used_bits(blocks_required);
    size_t index = find_optimal_space(*blocks, levels);
    printf("allocating %zu at index %zu\n", blocks_required, index);
    if (index == (size_t)-1) {
        return NULL;
    }

    blocks_t mask = ((1 << min_blocks_required) - 1) << index;
    assert(((*blocks) & mask) == mask && "Not all blocks are free there");
    *blocks &= ~mask;
    
    unsigned char* ptr = memory + index * BLOCK_SIZE;
    #ifdef MEMORY_DEBUG
    memset(ptr, UNUSED_MEMORY_VALUE, min_blocks_required * BLOCK_SIZE);
    #endif
    return ptr;
}

void free_memory(blocks_t* blocks, void* ptr, size_t size) {
    size_t min_blocks_required = ceil_divide(size, BLOCK_SIZE);
    size_t index = ((char*)ptr - (char*)memory) / BLOCK_SIZE;
    size_t mask = ((1 << min_blocks_required) - 1) << index;
    #ifdef MEMORY_DEBUG
    assert(((*blocks) & mask) == 0 && "Memory with the same size isn't allocated at this address.");
    size_t remaining_size = (min_blocks_required * BLOCK_SIZE) - size;
     
    for (size_t i = 0; i < remaining_size; i++) {
        assert(((unsigned char*)ptr)[size + i] == UNUSED_MEMORY_VALUE && "Used invalid memory");
    }
    #endif

    (*blocks) |= mask;
}

int main() {
    blocks_t blocks = BLOCKS_ONES;
    // ones where its occupied
    // blocks_t blocks = ~0x00F39000;
    // 00 00  00 00  11 11  00 11  10 01  00 00  00 00  00 00
    
    printf("blocks: %08X\n", blocks);
    void* a = allocate(&blocks, 88);
    memset(a, 1, 88);
    printf("blocks: %08X\n", blocks);
    void* b = allocate(&blocks, 124);
    memset(b, 2, 124);
    printf("blocks: %08X\n", blocks);
    void* c = allocate(&blocks, 56);
    memset(c, 4, 56);
    printf("blocks: %08X\n", blocks);
    void* d = allocate(&blocks, 104);
    memset(d, 3, 104);
    printf("blocks: %08X\n", blocks);
    
    assert(allocate(&blocks, 102) == NULL);
    free_memory(&blocks, a, 88);
    printf("blocks: %08X\n", blocks);
    
    allocate(&blocks, 8);
    printf("blocks: %08X\n", blocks);
    return 0;
}
