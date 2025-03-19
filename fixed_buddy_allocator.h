#ifndef FIXED_BUDDY_ALLOCATOR_H
#define FIXED_BUDDY_ALLOCATOR_H
#include <stddef.h>

#ifndef FBA_ASSERT
#include <assert.h>
#define FBA_ASSERT assert
#endif // FBA_ASSERT

#ifndef FBA_UNSUDED_MEMORY_VALUE
#define FBA_UNUSED_MEMORY_VALUE 0xCD
#endif // FBA_UNUSED_MEMORY_VALUE

#ifndef FBA_BLOCK_SIZE
#define FBA_BLOCK_SIZE 16
#endif // FBA_BLOCK_SIZE
typedef unsigned int blocks_t;
#define FBA_BLOCK_COUNT (sizeof(blocks_t) * CHAR_BIT)

typedef struct FixedBuddyAllocator {
    blocks_t blocks;
    unsigned char* buffer;
} FixedBuddyAllocator;

// the buffer must be of size FBA_BLOCK_SIZE * FBA_BLOCK_COUNT
FixedBuddyAllocator fba_create(unsigned char* buffer);
void* fba_allocate(FixedBuddyAllocator* allocator, size_t size);
void fba_free(FixedBuddyAllocator* allocator, void* ptr, size_t size);
#endif // FIXED_BUDDY_ALLOCATOR_H

#ifdef FBA_IMPLEMENTATION
#include <limits.h>
#include <string.h>



static size_t _fba_ceil_divide(size_t a, size_t b) {
    return (a + b - 1) / b;
}

static unsigned int _fba_count_leading_zeros(unsigned int num) {
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

static unsigned int _fba_count_trailing_zeros(unsigned int num) {
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
static unsigned int _fba_used_bits(unsigned int num) {
    return sizeof(unsigned int) * CHAR_BIT - _fba_count_leading_zeros(num) - 1;
}

static size_t _fba_round_up_to_next_power_of_two(size_t num) {
    num--;
    num |= num >> 1;
    num |= num >> 2;
    num |= num >> 4;
    num |= num >> 8;
    num |= num >> 16;
    num++;
    return num;
}
static size_t _fba_find_optimal_space(blocks_t blocks, size_t levels) {
    FBA_ASSERT(levels < 5);
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
        // filling the entire level with 1 where the pairs are 1
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
        return _fba_count_trailing_zeros(where);
    }
}

FixedBuddyAllocator fba_create(unsigned char* buffer) {
    return (FixedBuddyAllocator) {
        .buffer = buffer,
        .blocks = (blocks_t)-1
    };
}

void* fba_allocate(FixedBuddyAllocator* allocator, size_t size) {
    size_t min_blocks_required = _fba_ceil_divide(size, FBA_BLOCK_SIZE);
    size_t blocks_required = _fba_round_up_to_next_power_of_two(min_blocks_required);
    size_t levels = _fba_used_bits(blocks_required);
    size_t index = _fba_find_optimal_space(allocator->blocks, levels);
    if (index == (size_t)-1) {
        return NULL;
    }

    blocks_t mask = ((1 << min_blocks_required) - 1) << index;
    FBA_ASSERT((allocator->blocks & mask) == mask && "Not all blocks are free there");
    allocator->blocks &= ~mask;
    
    unsigned char* ptr = allocator->buffer + index * FBA_BLOCK_SIZE;
    #ifdef FBA_DEBUG
    memset(ptr, FBA_UNUSED_MEMORY_VALUE, min_blocks_required * FBA_BLOCK_SIZE);
    #endif
    return ptr;
}

void fba_free(FixedBuddyAllocator* allocator, void* ptr, size_t size) {
    size_t min_blocks_required = _fba_ceil_divide(size, FBA_BLOCK_SIZE);
    size_t index = ((unsigned char*)ptr - allocator->buffer) / FBA_BLOCK_SIZE;
    size_t mask = ((1 << min_blocks_required) - 1) << index;
    #ifdef FBA_DEBUG
    FBA_ASSERT((allocator->blocks & mask) == 0 && "Memory with the same size isn't allocated at this address.");
    size_t remaining_size = (min_blocks_required * FBA_BLOCK_SIZE) - size;
     
    for (size_t i = 0; i < remaining_size; i++) {
        FBA_ASSERT(((unsigned char*)ptr)[size + i] == FBA_UNUSED_MEMORY_VALUE && "Used invalid memory");
    }
    memset(ptr, FBA_UNUSED_MEMORY_VALUE, size);
    #endif

    allocator->blocks |= mask;
}

#endif // FBA_IMPLEMENTATION
