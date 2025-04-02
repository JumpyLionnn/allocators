# Allocators
Just a collection of various memory allocators I made in c
NOTE: They are barely tested

currently only posix based platforms are supported.

## Fixed Buddy allocator 
A fixed size buddy allocator managed using bit manipulation
This is just a proof of concept implementation

In both allocation and deallocation the size of the allocation should be known.
This is done to reduce the memory overhead of the allocator and to keep things simple.

The `blocks_t` type is an unsigned integer which stores the allocator state

each allocation is guaranteed to be aligned to the power of 2 size above it(assuming the memory block is aligned)
allocation: O(log2(m)) where m is the amount of bits in the internal storage
deallocation: o(1)

## Free list allocator
An general purpose allocator which works by keeping a linked list of free nodes.
Finding nodes is done using the best-fit strategy.
allocation: O(n) where n is the amount of free blocks
deallocation: O(1)
