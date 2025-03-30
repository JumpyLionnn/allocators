#ifndef FREE_LIST_ALLOCATOR_H
#define FREE_LIST_ALLOCATOR_H
#include <stddef.h>
#include <stdalign.h>
#include <assert.h>
#include <stdio.h>

typedef struct FreeListNodeHead {
    size_t size;
    int is_free;
} FreeListNodeHead;
typedef struct FreeListNodeTail {
    size_t size;
} FreeListNodeTail;

typedef struct FreeListAllocator {
    void* memory;
    size_t max_size;
    size_t page_size;
    size_t size;
    FreeListNodeHead* first_head;
} FreeListAllocator;

union _FlaRequiredAlignmentUnion {
    char head[alignof(FreeListNodeHead)];
    char tail[alignof(FreeListNodeTail)];
};

// including zero
#define FLA_IS_POWER_OF_TWO(num) ((num & (num - 1)) == 0)
#define FLA_MIN_ALIGNMENT (sizeof(union _FlaRequiredAlignmentUnion))
_Static_assert(FLA_IS_POWER_OF_TWO(FLA_MIN_ALIGNMENT), "Unable to fit alignment requiremenets");
#define FLA_MIN_ALLOCATION FLA_MIN_ALIGNMENT

FreeListAllocator fla_create(size_t max_size);

void* fla_allocate(FreeListAllocator* allocator, size_t size);
void fla_free(FreeListAllocator* allocator, void* ptr);
void fla_dump_nodes(FreeListAllocator* allocator);

#endif // FREE_LIST_ALLOCATOR_H
#ifdef FLA_IMPLEMENTATION

#define _FLA_NODE_MARGIN (sizeof(FreeListNodeHead) + sizeof(FreeListNodeTail))

#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
#include <unistd.h>
#include <sys/mman.h>
size_t _fla_get_page_size() {
    return sysconf(_SC_PAGE_SIZE);
}

void* _fla_reserve_memory(size_t size) {
    void* block = mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_NORESERVE | MAP_ANONYMOUS, -1, 0);
    if (block == (void*)-1) {
        return NULL;
    }
    return block;
}

void* _fla_allocate_memory_pages(void* ptr, size_t size) {
    void* block = mmap(ptr, size, PROT_WRITE | PROT_READ, MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (block == (void*)-1) {
        return NULL;
    }
    return block;
}

#endif // posix

static FreeListNodeHead* _fla_get_first_head(FreeListAllocator* allocator) {
    if (allocator->size == 0) {
        return NULL;
    }
    return (FreeListNodeHead*)allocator->memory;
}
static FreeListNodeHead* _fla_get_last_head(FreeListAllocator* allocator) {
    if (allocator->size == 0) {
        return NULL;
    }
    char* end = (char*)allocator->memory + allocator->size;
    FreeListNodeTail* tail = (FreeListNodeTail*)(end - sizeof(FreeListNodeTail));
    char* head_addr = (char*)tail - tail->size - sizeof(FreeListNodeHead);
    FreeListNodeHead* head = (FreeListNodeHead*)head_addr;
    assert(head->size == tail->size);
    return head;
}

// rounds a number up to a power of two number
static size_t _fla_round_up_to(size_t num, size_t to) {
    assert(FLA_IS_POWER_OF_TWO(to));
    size_t mask = to - 1;
    return (num + mask) & (~mask);
}

static FreeListNodeHead* _fla_find_atleast_best_fit(FreeListAllocator* allocator, size_t size) {
    FreeListNodeHead* current = _fla_get_first_head(allocator);
    if (current == NULL) {
        return NULL;
    }
    char* end = (char*)allocator->memory + allocator->size;
    FreeListNodeHead* best = NULL;
    size_t best_size = 0;
    while (1) {
        if (current->is_free && current->size >= size) {
            if (current->size < best_size || best == NULL) {
                best = current;
                if (current->size == size) {
                    break;
                }
            }
        }
        char* next_address = (char*)current + current->size + _FLA_NODE_MARGIN;
        if (end <= next_address) {
            break;
        }
        current = (FreeListNodeHead*)next_address;
    }

    return best;
}

static FreeListNodeHead* _fla_request_more_memory(FreeListAllocator* allocator, size_t required_size) {
    FreeListNodeHead* last = _fla_get_last_head(allocator);
    // size_t required_size = size;
    if (last != NULL && last->is_free) {
        required_size -= last->size;
    }
    else {
        required_size += _FLA_NODE_MARGIN;
    }

    size_t rounded_size = _fla_round_up_to(required_size, allocator->page_size);
    size_t min_node_size = _FLA_NODE_MARGIN + FLA_MIN_ALLOCATION;
    size_t added_size = rounded_size - required_size;
    if (added_size < min_node_size && added_size != 0) {
        rounded_size += allocator->page_size; 
    }
    required_size = rounded_size;
    if (allocator->size + required_size > allocator->max_size) {
        return NULL;
    }
    char* end =  (char*)allocator->memory + allocator->size;
    void* new_block = _fla_allocate_memory_pages(end, required_size);
    if (new_block == NULL) {
        return NULL;
    }
    allocator->size += required_size; 
    char* tail_addr = (char*)new_block + required_size - sizeof(FreeListNodeTail);
    FreeListNodeTail* tail = (FreeListNodeTail*)tail_addr;
    if (last != NULL && last->is_free) {
        assert(new_block == (char*)last + _FLA_NODE_MARGIN + last->size);
        last->size += required_size;
        tail->size = last->size;
    }
    else {
        last = (FreeListNodeHead*)new_block;
        last->is_free = 1;
        last->size = required_size - sizeof(FreeListNodeHead) - sizeof(FreeListNodeTail);
        tail->size = last->size;
    }
    return last;
}

FreeListNodeTail* _fla_get_node_tail(FreeListNodeHead* node) {
    return (FreeListNodeTail*)((char*)node + sizeof(FreeListNodeHead) + node->size);
}

FreeListNodeHead* _fla_get_next_node_head(FreeListAllocator* allocator, FreeListNodeHead* head) {
    char* address = (char*)head + head->size + _FLA_NODE_MARGIN;
    if (address > (char*)allocator->memory + allocator->size) {
        return NULL;
    }
    return (FreeListNodeHead*)address;
}
FreeListNodeHead* _fla_get_previous_node_head(FreeListAllocator* allocator, FreeListNodeHead* head) {
    FreeListNodeTail* prev_tail = (FreeListNodeTail*)((char*)head - sizeof(FreeListNodeTail));
    if ((char*)prev_tail < (char*)allocator->memory) {
        return NULL;
    }
    char* address = (char*)head - prev_tail->size - _FLA_NODE_MARGIN;
    assert(address >= (char*)allocator->memory);
    return (FreeListNodeHead*)address;
}

void* _fla_get_memory_ptr(FreeListNodeHead* head) {
    return (char*)head + sizeof(FreeListNodeHead);
}

FreeListNodeHead* _fla_get_head_from_ptr(void* ptr) {
    return (FreeListNodeHead*)((char*)ptr - sizeof(FreeListNodeHead));
}
void _fla_new_node(void* start, size_t size, int is_free) {
    FreeListNodeHead* head = start;
    head->size = size;
    head->is_free = is_free;
    FreeListNodeTail* tail = _fla_get_node_tail(head);
    tail->size = size;
}

FreeListAllocator fla_create(size_t max_size) {
    assert(max_size > 0);
    void* memory = _fla_reserve_memory(max_size);
    assert(memory != NULL);
    return (FreeListAllocator) {
        .memory = memory,
        .max_size = max_size,
        .page_size = _fla_get_page_size(),
        .size = 0,
    };
}


void* fla_allocate(FreeListAllocator* allocator, size_t size) {
    if (size == 0) {
        return NULL;
    }
    size = _fla_round_up_to(size, FLA_MIN_ALIGNMENT);
    FreeListNodeHead* node = _fla_find_atleast_best_fit(allocator, size);
    if (node == NULL) {
        node = _fla_request_more_memory(allocator, size);
        if (node == NULL) {
            return NULL;
        }
    }
    assert(node->size >= size);
    if (node->size >= size + _FLA_NODE_MARGIN + FLA_MIN_ALLOCATION) {
        size_t total_size = node->size;
        
        _fla_new_node(node, size, 0);

        FreeListNodeHead* next = _fla_get_next_node_head(allocator, node);
        assert(next != NULL);
        _fla_new_node(next, total_size - size - _FLA_NODE_MARGIN, 1);
    }
    else {
        node->is_free = 0;
    }
    return _fla_get_memory_ptr(node);
}

void fla_free(FreeListAllocator* allocator, void* ptr) {
    FreeListNodeHead* head = _fla_get_head_from_ptr(ptr);
    FreeListNodeTail* tail = _fla_get_node_tail(head);
    assert(head->size == tail->size);
    
    void* start = head;
    size_t size = head->size;

    FreeListNodeHead* prev = _fla_get_previous_node_head(allocator, head);
    if (prev != NULL && prev->is_free) {
        start = prev;
        size += prev->size + _FLA_NODE_MARGIN;
    }

    FreeListNodeHead* next = _fla_get_next_node_head(allocator, head);
    if (next != NULL && next->is_free) {
        size += next->size + _FLA_NODE_MARGIN;
    }

    _fla_new_node(start, size, 1);
}
void fla_dump_nodes(FreeListAllocator* allocator) {
    FreeListNodeHead* current = _fla_get_first_head(allocator);
    if (current == NULL) {
        return;
    }
    char* end = (char*)allocator->memory + allocator->size;
    while (1) {
        printf("%p: size: %zu, total size: %zu, free: %d\n", (void*)current, current->size, current->size + _FLA_NODE_MARGIN, current->is_free);
        char* next_address = (char*)current + current->size + _FLA_NODE_MARGIN;
        if (end <= next_address) {
            break;
        }
        current = (FreeListNodeHead*)next_address;
    }
}

#endif // FLA_IMPLEMENTATION
