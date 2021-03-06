#include "nova_renderer/memory/block_allocation_strategy.hpp"

#include <rx/core/memory/allocator.h>

#include "nova_renderer/memory/allocation_structs.hpp"

#include "../util/memory_utils.hpp"

using namespace nova::mem::operators;

namespace nova::mem {
    BlockAllocationStrategy::BlockAllocationStrategy(rx::memory::allocator* allocator_in, const Bytes size, const Bytes alignment_in)
        : memory_size(size), alignment(alignment_in) {

        allocator = allocator_in;

        head = make_new_block(0_b, size);
    }

    BlockAllocationStrategy::~BlockAllocationStrategy() {
        Block* next = nullptr;
        Block* current = head;

        while(current) {
            if(current->next) {
                next = current->next;
            }

            allocator->deallocate(reinterpret_cast<rx_byte*>(current));

            current = next;
        }
    }

    bool BlockAllocationStrategy::allocate(Bytes size, AllocationInfo& allocation) {
        size = align(size, alignment);

        const Bytes free_size = memory_size - allocated;
        if(free_size < size) {
            return false;
        }

        Block* best_fit = nullptr;

        for(Block* current = head; current; current = current->next) {
            if(!current->free || current->size < size) {
                continue;
            }

            if(best_fit == nullptr) {
                best_fit = current;
                break;
            }
        }

        if(!best_fit) {
            return false;
        }

        if(best_fit->size > size) {
            Block* block = make_new_block(best_fit->offset + size, best_fit->size - size);

            block->next = best_fit->next;
            block->previous = best_fit;
            best_fit->next = block;
        }

        allocated += size;

        allocation.size = size;
        allocation.offset = best_fit->offset;
        allocation.internal_data = best_fit;

        return true;
    }

    void BlockAllocationStrategy::free(const AllocationInfo& alloc) {
        auto* block = static_cast<Block*>(alloc.internal_data);
        block->free = true;

        if(block->previous && block->previous->free) {
            // We aren't the first block in the list, and the previous block is free. Merge this block into the previous block
            Block* prev = block->previous;

            prev->next = block->next;
            if(block->next) {
                block->next->previous = prev;
            }

            prev->size += block->size;

            allocator->deallocate(reinterpret_cast<rx_byte*>(block));

            block = prev;
        }

        if(block->next && block->next->free) {
            // There's a block right after us in the list, and it's free. Merge the next block into this block
            Block* next = block->next;

            if(next->next) {
                next->next->previous = block;
            }

            block->next = next->next;
            block->size += next->size;

            allocator->deallocate(reinterpret_cast<rx_byte*>(next));
        }

        allocated -= alloc.size;
    }

    BlockAllocationStrategy::Block* BlockAllocationStrategy::make_new_block(const Bytes offset, const Bytes size) {
        auto* block = allocator->create<Block>();
        block->id = next_block_id;
        block->size = size;
        block->offset = offset;

        next_block_id++;

        return block;
    }
} // namespace nova::mem
