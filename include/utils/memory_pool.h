#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include <vector>
#include <cstddef> // For size_t, std::byte, std::max_align_t
#include <memory>  // For std::unique_ptr if managing blocks
#include <stdexcept> // For std::runtime_error, std::bad_alloc

// Define cache line size (common value, but can vary by architecture)
// On C++17 and later, std::hardware_destructive_interference_size can be used.
#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif

// Forward declaration for NUMA node handling if we had a NUMA utility library
// struct NumaNode; 

class MemoryPool {
public:
    // Constructor:
    // object_size: The size of objects this pool will allocate.
    // initial_capacity: Number of objects to pre-allocate space for initially.
    // numa_node_id: Optional NUMA node to allocate memory from. -1 means default/any.
    MemoryPool(size_t object_size, size_t initial_capacity = 1024, int numa_node_id = -1);

    ~MemoryPool();

    // Allocate memory for a single object.
    // Returns a pointer to the allocated memory.
    // Throws std::bad_alloc if no memory is available and pool cannot grow.
    void* allocate();

    // Deallocate memory previously allocated by this pool.
    // ptr: Pointer to the memory to deallocate.
    void deallocate(void* ptr);

    // --- Configuration & Utility ---
    size_t getObjectSize() const { return actual_object_size; }
    size_t getTotalCapacity() const { return current_capacity_objects; } // Total objects pool can hold
    size_t getUsedCount() const;   // Number of currently allocated objects
    int getNumaNodeId() const { return numa_node; }
    bool isCacheAligned() const { return cache_align_objects; }

    // --- Advanced Features (Placeholders) ---
    // Grow the memory pool by a certain number of objects.
    bool grow(size_t additional_objects);

    // Release empty blocks of memory back to the system (if supported).
    void shrinkToFit();


private:
    struct Block {
        std::byte* memory; // Raw memory block
        size_t capacity_bytes; // Capacity of this block in bytes
        // Potentially: NumaNode* numa_info; for NUMA-specific block management

        Block(size_t bytes, int numa_node_id = -1, bool align_to_cache_line = false);
        ~Block();

        // Disable copy/move for simplicity in this skeleton
        Block(const Block&) = delete;
        Block& operator=(const Block&) = delete;
        Block(Block&&) = delete;
        Block& operator=(Block&&) = delete;
    };

    size_t requested_object_size; // Size requested by user
    size_t actual_object_size;    // Size after alignment considerations
    size_t current_capacity_objects; // Total objects the pool can currently hold across all blocks
    
    std::vector<std::unique_ptr<Block>> memory_blocks; // List of large memory blocks
    std::byte* free_list_head; // Head of the singly linked list of free memory slots

    int numa_node; // Target NUMA node for allocations
    bool cache_align_objects; // Whether objects should be cache line aligned

    // Helper to add a new block of memory to the pool
    bool addNewBlock(size_t num_objects_in_block);

    // Helper to align size or pointers
    static size_t alignUp(size_t size, size_t alignment);
    static void* alignPointer(void* ptr, size_t alignment);

    // NUMA allocation placeholder (would require OS-specific calls)
    static std::byte* allocateNumaMemory(size_t bytes, int numa_node_id, bool align_to_cache_line);
    static void deallocateNumaMemory(std::byte* memory, size_t bytes, int numa_node_id);
};

// Template version for type safety (optional, but good practice)
template <typename T>
class TypedMemoryPool : private MemoryPool {
public:
    TypedMemoryPool(size_t initial_capacity = 1024, int numa_node_id = -1)
        : MemoryPool(sizeof(T), initial_capacity, numa_node_id) {}

    T* allocateTyped() {
        return static_cast<T*>(allocate());
    }

    void deallocateTyped(T* ptr) {
        deallocate(static_cast<void*>(ptr));
    }

    // Expose relevant non-modifying methods from base
    size_t getObjectSize() const { return MemoryPool::getObjectSize(); }
    size_t getTotalCapacity() const { return MemoryPool::getTotalCapacity(); }
    size_t getUsedCount() const { return MemoryPool::getUsedCount(); }
    int getNumaNodeId() const { return MemoryPool::getNumaNodeId(); }
    bool isCacheAligned() const { return MemoryPool::isCacheAligned(); }
};


#endif // MEMORY_POOL_H
