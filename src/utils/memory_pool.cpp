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
    std::cout << "allocateNumaMemory (Placeholder): Requesting " << bytes << " bytes. NUMA node: " << numa_node_id 
              << ", Align: " << align_to_cache_line << std::endl;
    // Simple non-NUMA, potentially aligned allocation for skeleton
    void* mem = nullptr;
    size_t alignment = align_to_cache_line ? CACHE_LINE_SIZE : alignof(std::max_align_t);

    // C++17 std::aligned_alloc (not available in C++11/14 directly in std lib, but _aligned_malloc etc. exist)
    // For this skeleton, using malloc and warning if alignment is not guaranteed.
    // A real implementation would use platform-specific aligned allocation.
    if (align_to_cache_line || alignment > alignof(std::max_align_t)) {
        // Using malloc here is a simplification. Real code needs aligned_alloc or platform equivalent.
        // mem = std::aligned_alloc(alignment, bytes); // C11, C++17 (stdlib)
        // For older C++ or broader compatibility:
        // mem = _mm_malloc(bytes, alignment); // Intel intrinsics
        // posix_memalign(&mem, alignment, bytes); // POSIX
        std::cout << "Warning: Using basic malloc. Cache line alignment (" << alignment << "B) might not be guaranteed by this skeleton." << std::endl;
        mem = std::malloc(bytes); // Basic allocation
    } else {
        mem = std::malloc(bytes);
    }

    if (!mem) {
        std::cerr << "Error: std::malloc failed to allocate " << bytes << std::endl;
        return nullptr;
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
