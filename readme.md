# Fixed Buddy allocator 
This repository contains a fixed size buddy allocator managed using bit manipulation
This is just a proof of concept implementation

In both allocation and deallocation the size of the allocation should be known.
This is done to reduce the memory overhead of the allocator and to keep things simple.

The `blocks_t` type is an unsigned integer which stores the allocator state

each allocation is guaranteed to be aligned to the power of 2 size above it(assuming the memory block is aligned)
