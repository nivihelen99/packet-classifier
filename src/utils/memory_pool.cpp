#include "utils/memory_pool.h"
#include <iostream> // For placeholder output
#include <cstdlib>  // For std::malloc, std::free, std::aligned_alloc (if available and chosen)
#include <algorithm> // For std::max

// --- MemoryPool::Block Implementation ---
MemoryPool::Block::Block(size_t bytes, int numa_node_id, bool align_to_cache_line)
    : memory(nullptr), capacity_bytes(bytes) {
    std::cout << "Block: Allocating " << bytes << " bytes. NUMA node: " 
              << numa_node_id << ", Cache align: " << align_to_cache_line << std::endl;
    
    // Placeholder for NUMA-aware and cache-aligned allocation
    memory = MemoryPool::allocateNumaMemory(bytes, numa_node_id, align_to_cache_line);
    if (!memory) {
        throw std::bad_alloc();
    }
    // Optionally, initialize memory (e.g., for debugging, or to pre-fault pages)
}

MemoryPool::Block::~Block() {
    std::cout << "Block: Deallocating " << capacity_bytes << " bytes." << std::endl;
    if (memory) {
        // Placeholder for NUMA-aware deallocation
        MemoryPool::deallocateNumaMemory(memory, capacity_bytes, -1 /* numa_node info might be needed */);
        memory = nullptr;
    }
}

// --- MemoryPool Helper Functions ---
size_t MemoryPool::alignUp(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

void* MemoryPool::alignPointer(void* ptr, size_t alignment) {
    uintptr_t int_ptr = reinterpret_cast<uintptr_t>(ptr);
    uintptr_t aligned_ptr = (int_ptr + alignment - 1) & ~(alignment - 1);
    return reinterpret_cast<void*>(aligned_ptr);
}

// Placeholder for actual NUMA / Aligned Allocation
// On Linux, this might use numa_alloc_onnode / numa_free or mbind/set_mempolicy + aligned_alloc.
// On Windows, VirtualAllocExNuma + HeapAlloc or _aligned_malloc.
std::byte* MemoryPool::allocateNumaMemory(size_t bytes, int numa_node_id, bool align_to_cache_line) {
    // std::cout << "allocateNumaMemory: Requesting " << bytes << " bytes. NUMA node: " << numa_node_id 
    //           << ", Align: " << align_to_cache_line << std::endl;
    void* mem = nullptr;
    size_t alignment_final = align_to_cache_line ? CACHE_LINE_SIZE : alignof(std::max_align_t);

    if (bytes == 0) return nullptr; 

    // posix_memalign requires alignment to be a power of two and a multiple of sizeof(void*).
    // Step 1: Ensure it's a power of two.
    if ((alignment_final & (alignment_final - 1)) != 0 || alignment_final == 0) { 
        size_t pot_alignment = 1;
        // Use original requested alignment (CACHE_LINE_SIZE or max_align_t) as base for finding next power of two
        size_t base_for_pot = align_to_cache_line ? CACHE_LINE_SIZE : alignof(std::max_align_t);
        while(pot_alignment < base_for_pot) {
            pot_alignment <<= 1;
            if (pot_alignment == 0) { // Overflow before reaching base_for_pot, highly unlikely
                alignment_final = CACHE_LINE_SIZE; // Fallback
                goto alignment_set; // Jump past further power-of-two logic
            }
        }
        alignment_final = pot_alignment;
    }
  alignment_set:;

    // Step 2: Ensure it's a multiple of sizeof(void*).
    if (alignment_final % sizeof(void*) != 0) {
        alignment_final = (alignment_final + sizeof(void*) - 1) & ~(sizeof(void*) - 1);
        // Re-check power of two, as aligning to sizeof(void*) might make it non-power-of-two
        // if sizeof(void*) is not a power of two (very unlikely for typical systems).
        if ((alignment_final & (alignment_final - 1)) != 0 || alignment_final == 0) {
             size_t pot_alignment = 1;
             size_t base_for_pot = align_to_cache_line ? CACHE_LINE_SIZE : alignof(std::max_align_t);
             base_for_pot = std::max(base_for_pot, sizeof(void*)); // must be at least sizeof(void*)
             while(pot_alignment < base_for_pot) {
                pot_alignment <<= 1;
                if (pot_alignment == 0) { alignment_final = CACHE_LINE_SIZE; goto final_alignment_decision;}
             }
             alignment_final = pot_alignment;
        }
    }
  final_alignment_decision:;
    // Ensure a minimum valid alignment if all else failed (e.g. if CACHE_LINE_SIZE was weirdly 0)
    if (alignment_final == 0 || (alignment_final & (alignment_final -1)) != 0 || alignment_final % sizeof(void*) != 0) {
        alignment_final = std::max(alignof(std::max_align_t), sizeof(void*));
        // ensure power of two again for this ultimate fallback
        if((alignment_final & (alignment_final -1)) != 0) {
            size_t pot_align = 1; while(pot_align < alignment_final) pot_align <<=1; alignment_final = pot_align;
        }
    }


    // Use posix_memalign if specific cache line alignment was requested,
    // or if the natural alignment (max_align_t) is smaller than what we calculated (e.g. due to sizeof(void*) adjustment).
    // Generally, if alignment_final > what malloc guarantees (roughly alignof(std::max_align_t)), use posix_memalign.
    if (align_to_cache_line || alignment_final > alignof(std::max_align_t)) { 
        if (posix_memalign(&mem, alignment_final, bytes) != 0) {
            std::cerr << "Error: posix_memalign failed to allocate " << bytes 
                      << " with alignment " << alignment_final << std::endl;
            mem = nullptr; 
        }
    } else {
        // Standard malloc typically guarantees alignment for any fundamental type (i.e., alignof(std::max_align_t))
        mem = std::malloc(bytes); 
        if (!mem) {
             std::cerr << "Error: std::malloc failed to allocate " << bytes << std::endl;
        }
    }

    if (!mem) {
        return nullptr; // Block constructor will throw bad_alloc
    }
    return static_cast<std::byte*>(mem);
}

void MemoryPool::deallocateNumaMemory(std::byte* memory, size_t /*bytes*/, int /*numa_node_id*/) {
    std::cout << "deallocateNumaMemory (Placeholder): Deallocating memory." << std::endl;
    // std::free(memory); // If using std::aligned_alloc or posix_memalign
    // _mm_free(memory); // If using _mm_malloc
    std::free(memory); // Matches std::malloc
}


// --- MemoryPool Constructor & Destructor ---
MemoryPool::MemoryPool(size_t object_size, size_t initial_capacity, int numa_node_id)
    : requested_object_size(object_size),
      actual_object_size(0), // Will be set after alignment
      current_capacity_objects(0),
      free_list_head(nullptr),
      numa_node(numa_node_id),
      cache_align_objects(true) /* Default to cache align, can be configurable */ {

    if (object_size == 0) {
        throw std::invalid_argument("Object size cannot be zero.");
    }
    if (initial_capacity == 0) {
        throw std::invalid_argument("Initial capacity cannot be zero.");
    }

    // Determine actual object size considering cache line alignment
    // Each object slot in the free list will store a pointer to the next free slot.
    // So, object_size must be at least sizeof(void*).
    size_t min_size = std::max(requested_object_size, sizeof(std::byte*));
    if (cache_align_objects) {
        actual_object_size = alignUp(min_size, CACHE_LINE_SIZE);
    } else {
        actual_object_size = alignUp(min_size, alignof(std::max_align_t)); // Align to max natural alignment
    }
    
    std::cout << "MemoryPool: Requested obj size: " << requested_object_size
              << ", Actual obj size (aligned): " << actual_object_size
              << ", Initial capacity (objects): " << initial_capacity
              << ", NUMA Node: " << numa_node
              << ", Cache Aligned: " << cache_align_objects << std::endl;

    if (!addNewBlock(initial_capacity)) {
        // Constructor fails if initial block cannot be allocated
        throw std::bad_alloc(); // Or a more specific error
    }
}

MemoryPool::~MemoryPool() {
    std::cout << "MemoryPool: Destroying. All blocks will be deallocated." << std::endl;
    // memory_blocks unique_ptrs will automatically deallocate their managed Blocks.
    memory_blocks.clear();
    free_list_head = nullptr; // Not strictly necessary but good practice.
}

// --- Private Helper: addNewBlock ---
bool MemoryPool::addNewBlock(size_t num_objects_in_block) {
    if (num_objects_in_block == 0) return true; // Nothing to do

    size_t block_size_bytes = num_objects_in_block * actual_object_size;
    std::cout << "MemoryPool: Adding new block for " << num_objects_in_block 
              << " objects (" << block_size_bytes << " bytes)." << std::endl;
    
    std::unique_ptr<Block> new_block_ptr;
    try {
        new_block_ptr = std::make_unique<Block>(block_size_bytes, numa_node, cache_align_objects);
    } catch (const std::bad_alloc& e) {
        std::cerr << "Error: Failed to allocate new block: " << e.what() << std::endl;
        return false;
    }

    // Link new objects into the free list
    std::byte* current_object_ptr = new_block_ptr->memory;
    for (size_t i = 0; i < num_objects_in_block; ++i) {
        // The memory for the object is interpreted as a pointer to the next free object.
        *reinterpret_cast<std::byte**>(current_object_ptr) = free_list_head;
        free_list_head = current_object_ptr;
        current_object_ptr += actual_object_size;
    }
    
    memory_blocks.push_back(std::move(new_block_ptr));
    current_capacity_objects += num_objects_in_block;
    return true;
}

// --- Public Methods ---
void* MemoryPool::allocate() {
    if (!free_list_head) {
        // Pool is empty, try to grow it.
        // Growth strategy: double current capacity or add a default large chunk.
        // For skeleton, let's try to add the initial capacity again.
        std::cout << "MemoryPool: Out of memory, attempting to grow." << std::endl;
        size_t objects_to_add = current_capacity_objects > 0 ? current_capacity_objects : 1024; // Simplified growth
        if (!addNewBlock(objects_to_add)) {
            std::cerr << "Error: MemoryPool failed to grow." << std::endl;
            throw std::bad_alloc();
        }
    }

    if (!free_list_head) { // Should not happen if addNewBlock succeeded or threw
         std::cerr << "Critical Error: free_list_head still null after successful growth attempt or initial setup." << std::endl;
         throw std::runtime_error("Memory pool internal error.");
    }

    // Retrieve an object from the free list
    std::byte* allocated_object = free_list_head;
    free_list_head = *reinterpret_cast<std::byte**>(free_list_head); // Point to the next free object

    // std::cout << "MemoryPool: Allocated object at " << static_cast<void*>(allocated_object) << std::endl;
    // Optionally, poison memory (e.g., for debugging)
    // std::fill_n(allocated_object, actual_object_size, static_cast<std::byte>(0xCD));
    return allocated_object;
}

void MemoryPool::deallocate(void* ptr) {
    if (!ptr) {
        std::cout << "MemoryPool: Attempted to deallocate a null pointer. Ignoring." << std::endl;
        return;
    }
    // std::cout << "MemoryPool: Deallocating object at " << ptr << std::endl;

    // Add the object back to the head of the free list
    std::byte* object_to_free = static_cast<std::byte*>(ptr);

    // Optionally, poison memory (e.g., for debugging)
    // std::fill_n(object_to_free, actual_object_size, static_cast<std::byte>(0xDD));

    *reinterpret_cast<std::byte**>(object_to_free) = free_list_head;
    free_list_head = object_to_free;
}

size_t MemoryPool::getUsedCount() const {
    // This is an estimation. A precise count requires iterating the free list or blocks.
    // For a simple pool, it's capacity - free_count.
    // Counting nodes in free_list:
    size_t free_count = 0;
    std::byte* current = free_list_head;
    while (current) {
        free_count++;
        current = *reinterpret_cast<std::byte**>(current);
    }
    return current_capacity_objects - free_count;
}

bool MemoryPool::grow(size_t additional_objects) {
    std::cout << "MemoryPool: Explicitly growing by " << additional_objects << " objects." << std::endl;
    return addNewBlock(additional_objects);
}

void MemoryPool::shrinkToFit() {
    std::cout << "MemoryPool: shrinkToFit (Placeholder). Not implemented in this skeleton." << std::endl;
    // This is complex. It would involve:
    // 1. Identifying blocks that are completely empty or mostly empty.
    // 2. Allocating new, smaller blocks.
    // 3. Moving active objects from old blocks to new blocks (invalidates pointers!).
    //    OR: Only freeing blocks where all objects are in the free list.
    // 4. Deallocating the old, now empty blocks.
    // This is generally not done for simple memory pools due to complexity and pointer invalidation.
}
