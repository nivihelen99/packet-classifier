#include "gtest/gtest.h"
#include "utils/memory_pool.h" // Adjust path as necessary
#include <vector>
#include <cstdint> // For uintptr_t
#include <numeric> // For std::iota
#include <algorithm> // For std::sort, std::unique

// Define CACHE_LINE_SIZE if not already defined (e.g., for consistency with memory_pool.cpp)
#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif

TEST(MemoryPoolTest, ConstructorValidParams) {
    MemoryPool pool(32, 100);
    size_t expected_actual_size = (std::max(32UL, sizeof(void*)) + CACHE_LINE_SIZE - 1) & ~(CACHE_LINE_SIZE - 1);
    EXPECT_EQ(pool.getObjectSize(), expected_actual_size);
    EXPECT_EQ(pool.getTotalCapacity(), 100);
    EXPECT_EQ(pool.getUsedCount(), 0);
    EXPECT_TRUE(pool.isCacheAligned()); // Default is true
}

TEST(MemoryPoolTest, ConstructorSmallObjectSize) {
    // Size 1 will be bumped to sizeof(void*) then aligned
    size_t expected_actual_size = (std::max(1UL, sizeof(void*)) + CACHE_LINE_SIZE - 1) & ~(CACHE_LINE_SIZE - 1);
    MemoryPool pool(1, 10);
    EXPECT_EQ(pool.getObjectSize(), expected_actual_size);
    EXPECT_EQ(pool.getTotalCapacity(), 10);
}

TEST(MemoryPoolTest, ConstructorZeroObjectSize) {
    EXPECT_THROW(MemoryPool pool(0, 10), std::invalid_argument);
}

TEST(MemoryPoolTest, ConstructorZeroInitialCapacity) {
    EXPECT_THROW(MemoryPool pool(16, 0), std::invalid_argument);
}

TEST(MemoryPoolTest, BasicAllocationDeallocation) {
    MemoryPool pool(64, 10);
    void* ptr1 = pool.allocate();
    ASSERT_NE(ptr1, nullptr);
    EXPECT_EQ(pool.getUsedCount(), 1);

    pool.deallocate(ptr1);
    EXPECT_EQ(pool.getUsedCount(), 0);

    void* ptr2 = pool.allocate(); // Should be able to reallocate
    ASSERT_NE(ptr2, nullptr);
    EXPECT_EQ(pool.getUsedCount(), 1);
    // It's likely ptr1 == ptr2 if it's a LIFO free list, but not guaranteed by all pool impls.
    // For this specific implementation (LIFO free list), they should be the same.
    EXPECT_EQ(ptr1, ptr2); 

    pool.deallocate(ptr2);
}

TEST(MemoryPoolTest, PoolExhaustion) {
    const size_t capacity = 5;
    MemoryPool pool(32, capacity);
    std::vector<void*> allocated_pointers;
    for (size_t i = 0; i < capacity; ++i) {
        void* ptr = pool.allocate();
        ASSERT_NE(ptr, nullptr);
        allocated_pointers.push_back(ptr);
    }
    EXPECT_EQ(pool.getUsedCount(), capacity);

    // Next allocation should attempt to grow.
    // The current growth strategy is to double or add initial_capacity.
    // So, it should succeed here.
    void* extra_ptr = nullptr;
    EXPECT_NO_THROW(extra_ptr = pool.allocate());
    ASSERT_NE(extra_ptr, nullptr);
    EXPECT_EQ(pool.getUsedCount(), capacity + 1);
    EXPECT_EQ(pool.getTotalCapacity(), capacity * 2); // Assuming it doubles

    pool.deallocate(extra_ptr);
    for (void* p : allocated_pointers) {
        pool.deallocate(p);
    }
}


TEST(MemoryPoolTest, PoolExhaustionThrowsWhenGrowthFails) {
    // This test is hard to reliably trigger without mocking underlying allocations.
    // We assume if addNewBlock fails (returns false), allocate() throws std::bad_alloc.
    // The current addNewBlock only returns false if std::make_unique<Block> fails,
    // which then throws bad_alloc itself. So allocate() will throw bad_alloc directly.
    // A more direct way to test this would be to create a pool, exhaust it,
    // then try to make the next system allocation fail (not feasible in unit test).
    // For now, we test that if initial allocation fails, constructor throws.
    // The provided source code for MemoryPool constructor already throws std::bad_alloc
    // if the initial addNewBlock fails.
    // So, we'll focus on the allocate() throwing after initial capacity is met AND growth fails.
    // The current growth strategy in the skeleton always tries to add blocks.
    // A true failure of growth would mean the system is out of memory.
    // We can simulate this by making a very large object size.
    // However, the test environment might still allocate it.
    
    // This test relies on the fact that an extremely large allocation WILL fail.
    // This might not be true on all systems or could take a very long time.
    // Skipping this specific type of exhaustion test for robustness in typical CI.
    GTEST_SKIP() << "Skipping specific growth failure test; hard to guarantee std::bad_alloc from growth in unit test.";
}


TEST(MemoryPoolTest, DeallocateAndReuse) {
    const size_t capacity = 10;
    MemoryPool pool(128, capacity);
    std::vector<void*> ptrs;
    for (size_t i = 0; i < capacity; ++i) {
        ptrs.push_back(pool.allocate());
    }
    EXPECT_EQ(pool.getUsedCount(), capacity);

    pool.deallocate(ptrs[0]);
    pool.deallocate(ptrs[2]);
    pool.deallocate(ptrs[5]);
    EXPECT_EQ(pool.getUsedCount(), capacity - 3);

    void* p0_new = pool.allocate();
    void* p2_new = pool.allocate();
    void* p5_new = pool.allocate();
    ASSERT_NE(p0_new, nullptr);
    ASSERT_NE(p2_new, nullptr);
    ASSERT_NE(p5_new, nullptr);
    EXPECT_EQ(pool.getUsedCount(), capacity);

    // Due to LIFO, expect specific re-allocation order
    EXPECT_EQ(p0_new, ptrs[5]);
    EXPECT_EQ(p2_new, ptrs[2]);
    EXPECT_EQ(p5_new, ptrs[0]);

    // Clean up
    pool.deallocate(p0_new);
    pool.deallocate(p2_new);
    pool.deallocate(p5_new);
    for (size_t i = 0; i < ptrs.size(); ++i) {
        if (i != 0 && i != 2 && i != 5) {
            pool.deallocate(ptrs[i]);
        }
    }
    EXPECT_EQ(pool.getUsedCount(), 0);
}

TEST(MemoryPoolTest, Alignment) {
    MemoryPool pool(33, 5); // Requested 33, actual will be 64 (CACHE_LINE_SIZE)
    EXPECT_TRUE(pool.isCacheAligned());
    EXPECT_EQ(pool.getObjectSize(), CACHE_LINE_SIZE);

    void* ptr = pool.allocate();
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % CACHE_LINE_SIZE, 0);
    pool.deallocate(ptr);

    // Test with cache_align_objects = false (not possible with current constructor)
    // If we had a constructor: MemoryPool(size, cap, numa, cache_align=false)
    // size_t expected_size_no_cache_align = (std::max(33UL, sizeof(void*)) + alignof(std::max_align_t) - 1) & ~(alignof(std::max_align_t) - 1);
    // MemoryPool pool_no_cache_align(33, 5, -1, false);
    // EXPECT_FALSE(pool_no_cache_align.isCacheAligned());
    // EXPECT_EQ(pool_no_cache_align.getObjectSize(), expected_size_no_cache_align);
    // ptr = pool_no_cache_align.allocate();
    // EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % alignof(std::max_align_t), 0);
    // pool_no_cache_align.deallocate(ptr);
}

TEST(MemoryPoolTest, DeallocateNullPtr) {
    MemoryPool pool(16, 10);
    EXPECT_NO_THROW(pool.deallocate(nullptr));
    EXPECT_EQ(pool.getUsedCount(), 0);
}

TEST(MemoryPoolTest, GrowPool) {
    MemoryPool pool(16, 5);
    EXPECT_EQ(pool.getTotalCapacity(), 5);
    
    for(size_t i=0; i<5; ++i) { ASSERT_NE(pool.allocate(), nullptr); }
    EXPECT_EQ(pool.getUsedCount(), 5);

    bool grown = pool.grow(10); // Explicitly grow
    EXPECT_TRUE(grown);
    EXPECT_EQ(pool.getTotalCapacity(), 5 + 10);
    EXPECT_EQ(pool.getUsedCount(), 5); // Used count unchanged

    // Allocate from newly grown part
    for(size_t i=0; i<10; ++i) { ASSERT_NE(pool.allocate(), nullptr); }
    EXPECT_EQ(pool.getUsedCount(), 15);
}

TEST(MemoryPoolTest, TypedMemoryPool) {
    struct MyStruct {
        int a;
        char b[20];
        double c;
    };
    // Default cache alignment will make object size larger than sizeof(MyStruct)
    size_t expected_actual_obj_size = (std::max(sizeof(MyStruct), sizeof(void*)) + CACHE_LINE_SIZE - 1) & ~(CACHE_LINE_SIZE - 1);

    TypedMemoryPool<MyStruct> typed_pool(10);
    EXPECT_EQ(typed_pool.getObjectSize(), expected_actual_obj_size);
    EXPECT_EQ(typed_pool.getTotalCapacity(), 10);
    EXPECT_TRUE(typed_pool.isCacheAligned());

    MyStruct* s_ptr1 = typed_pool.allocateTyped();
    ASSERT_NE(s_ptr1, nullptr);
    EXPECT_EQ(typed_pool.getUsedCount(), 1);
    
    // Check alignment
    EXPECT_EQ(reinterpret_cast<uintptr_t>(s_ptr1) % CACHE_LINE_SIZE, 0);

    s_ptr1->a = 10;
    s_ptr1->c = 3.14;

    typed_pool.deallocateTyped(s_ptr1);
    EXPECT_EQ(typed_pool.getUsedCount(), 0);

    MyStruct* s_ptr2 = typed_pool.allocateTyped();
    ASSERT_NE(s_ptr2, nullptr);
    EXPECT_EQ(s_ptr2, s_ptr1); // LIFO behavior

    typed_pool.deallocateTyped(s_ptr2);
}

TEST(MemoryPoolTest, UsedCountAccuracy) {
    MemoryPool pool(sizeof(int), 100);
    std::vector<void*> ptrs;

    // Allocate 50
    for(int i=0; i<50; ++i) ptrs.push_back(pool.allocate());
    EXPECT_EQ(pool.getUsedCount(), 50);

    // Deallocate 20
    for(int i=0; i<20; ++i) pool.deallocate(ptrs[i]);
    ptrs.erase(ptrs.begin(), ptrs.begin() + 20);
    EXPECT_EQ(pool.getUsedCount(), 30);

    // Allocate 30 more
    for(int i=0; i<30; ++i) ptrs.push_back(pool.allocate());
    EXPECT_EQ(pool.getUsedCount(), 60);

    // Deallocate all
    for(void* p : ptrs) pool.deallocate(p);
    EXPECT_EQ(pool.getUsedCount(), 0);
}

// int main(int argc, char **argv) {
//     ::testing::InitGoogleTest(&argc, argv);
//     return RUN_ALL_TESTS();
// }
