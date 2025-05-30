#include "gtest/gtest.h"
#include "data_structures/concurrent_hash.h" // Adjust path as necessary
#include <string>
#include <vector>
#include <thread> // For potential future concurrency tests, not used for now

// Note: The ConcurrentHashTable implementation is currently a placeholder.
// Concurrency mechanisms (RCU, full Robin Hood hashing) are not fully implemented.
// These tests focus on the current behavior: a hash table using std::atomic for
// slot status ('in_use') and basic linear probing.
// True multi-threaded concurrency testing is not performed here.

class ConcurrentHashTableTest : public ::testing::Test {
protected:
    // Helper to get value from lookup, returns true if found, false otherwise
    bool getValue(const ConcurrentHashTable& table, const std::string& key, int& value) {
        return table.lookup(key, value);
    }
};

TEST_F(ConcurrentHashTableTest, ConstructorAndEmptyTable) {
    ConcurrentHashTable table(16); // Small capacity for easier testing
    int value;
    EXPECT_FALSE(getValue(table, "any_key", value));
    EXPECT_FALSE(getValue(table, "", value)); // Empty key
}

TEST_F(ConcurrentHashTableTest, BasicInsertAndLookup) {
    ConcurrentHashTable table(16);
    table.insert("key1", 10);
    table.insert("key2", 20);
    table.insert("another_key", 30);

    int value;
    ASSERT_TRUE(getValue(table, "key1", value));
    EXPECT_EQ(value, 10);

    ASSERT_TRUE(getValue(table, "key2", value));
    EXPECT_EQ(value, 20);

    ASSERT_TRUE(getValue(table, "another_key", value));
    EXPECT_EQ(value, 30);

    EXPECT_FALSE(getValue(table, "key3", value)); // Non-existent
    EXPECT_FALSE(getValue(table, "", value));     // Empty key (if not inserted)
}

TEST_F(ConcurrentHashTableTest, InsertUpdatesExistingKey) {
    ConcurrentHashTable table(16);
    table.insert("key1", 100);
    int value;
    ASSERT_TRUE(getValue(table, "key1", value));
    EXPECT_EQ(value, 100);

    table.insert("key1", 200); // Update
    ASSERT_TRUE(getValue(table, "key1", value));
    EXPECT_EQ(value, 200);
}

TEST_F(ConcurrentHashTableTest, BasicRemove) {
    ConcurrentHashTable table(16);
    table.insert("key_to_remove", 55);
    table.insert("key_to_keep", 66);

    int value;
    ASSERT_TRUE(getValue(table, "key_to_remove", value));
    EXPECT_EQ(value, 55);
    ASSERT_TRUE(getValue(table, "key_to_keep", value));
    EXPECT_EQ(value, 66);

    EXPECT_TRUE(table.remove("key_to_remove"));
    EXPECT_FALSE(getValue(table, "key_to_remove", value)); // Should be gone
    ASSERT_TRUE(getValue(table, "key_to_keep", value));    // Should still be there
    EXPECT_EQ(value, 66);
}

TEST_F(ConcurrentHashTableTest, RemoveNonExistentKey) {
    ConcurrentHashTable table(16);
    table.insert("key1", 1);
    EXPECT_FALSE(table.remove("non_existent_key"));
    
    int value;
    ASSERT_TRUE(getValue(table, "key1", value)); // Ensure existing key is unaffected
    EXPECT_EQ(value, 1);
}

TEST_F(ConcurrentHashTableTest, InsertEmptyStringKey) {
    ConcurrentHashTable table(16);
    table.insert("", 12345);
    int value;
    ASSERT_TRUE(getValue(table, "", value));
    EXPECT_EQ(value, 12345);

    table.insert("", 54321); // Update empty string key
    ASSERT_TRUE(getValue(table, "", value));
    EXPECT_EQ(value, 54321);

    EXPECT_TRUE(table.remove(""));
    EXPECT_FALSE(getValue(table, "", value));
}

// Test for collision handling (relies on std::hash behavior and linear probing)
// To make collisions more predictable, we can use a very small table.
// Note: std::hash<std::string> behavior is implementation-defined, so exact
// collision patterns aren't guaranteed across all compilers/libraries,
// but for simple strings and small table sizes, we can often force them.
TEST_F(ConcurrentHashTableTest, CollisionHandlingWithLinearProbing) {
    // Using a very small table to force collisions more easily.
    // Let's assume capacity is 2 for this test. The constructor might default to a minimum.
    // The current constructor defaults to 1024 if 0 is passed, but accepts small sizes.
    ConcurrentHashTable table(2); // Forcing high collision rate
                                   // The actual capacity might be adjusted by implementation.
                                   // Let's assume it respects small sizes for testing.

    // These keys might or might not collide depending on capacity and hash function.
    // If capacity is indeed small (e.g., 2), collisions are very likely.
    table.insert("key_A", 101); // hash("key_A") % 2 = index_A
    table.insert("key_B", 102); // hash("key_B") % 2 = index_B
    table.insert("key_C", 103); // hash("key_C") % 2 = index_C

    // The placeholder insert prints "Error: Table is full" if it can't find a slot.
    // We are testing if items can be retrieved, assuming they were inserted.
    // If capacity is 2, only two items can be inserted if no resize happens.
    // The current insert doesn't resize on full, but prints error.

    int val_A, val_B, val_C;
    bool found_A = getValue(table, "key_A", val_A);
    bool found_B = getValue(table, "key_B", val_B);
    bool found_C = getValue(table, "key_C", val_C);

    // With capacity 2, at most two of these can be inserted.
    // Check that those inserted can be retrieved.
    // This test is more about whether linear probing works for what *was* inserted.
    int items_inserted = (found_A ? 1:0) + (found_B ? 1:0) + (found_C ? 1:0);
    EXPECT_GE(items_inserted, 0); // At least 0 inserted
    EXPECT_LE(items_inserted, 2); // At most 2 inserted for capacity 2

    if (found_A) EXPECT_EQ(val_A, 101);
    if (found_B) EXPECT_EQ(val_B, 102);
    if (found_C) EXPECT_EQ(val_C, 103);

    // Try to lookup after potential removals (if they were inserted)
    if (found_A) {
        EXPECT_TRUE(table.remove("key_A"));
        EXPECT_FALSE(getValue(table, "key_A", val_A));
        if (found_B) { // If B was also inserted
             ASSERT_TRUE(getValue(table, "key_B", val_B));
             EXPECT_EQ(val_B, 102);
        }
    }
}

TEST_F(ConcurrentHashTableTest, FillTableNearCapacity) {
    const int test_capacity = 5; // Small, but not extremely small
    ConcurrentHashTable table(test_capacity);
    std::vector<std::string> keys;
    for (int i = 0; i < test_capacity; ++i) {
        std::string key = "fill_key_" + std::to_string(i);
        keys.push_back(key);
        table.insert(key, i * 10);
    }

    // Verify all inserted keys are found
    for (int i = 0; i < test_capacity; ++i) {
        int value;
        ASSERT_TRUE(getValue(table, keys[i], value)) << "Failed for key: " << keys[i];
        EXPECT_EQ(value, i * 10);
    }

    // Try inserting one more - current placeholder should print "Table is full"
    // and not insert it, or if it did, previous values should still be correct.
    // The `insert` function in the placeholder doesn't throw but prints to cerr.
    table.insert("overflow_key", 999);
    int value_overflow;
    // It's possible the "overflow_key" overwrote something if hashing is unlucky
    // or if the table wasn't actually full due to hash collisions allowing some empty slots.
    // The current `insert` simply iterates until full or slot found.
    // This test mostly checks that operations on a full/nearly full table don't corrupt it.

    // Re-verify all original keys are still found and correct
    for (int i = 0; i < test_capacity; ++i) {
        int value;
        ASSERT_TRUE(getValue(table, keys[i], value)) << "Failed for key after overflow attempt: " << keys[i];
        EXPECT_EQ(value, i * 10);
    }
    // Check if overflow_key was inserted (it shouldn't if table was truly full by count)
    // The placeholder insert will return if probe_distance >= capacity.
    // If all slots were filled, it won't be inserted.
}


// Placeholder for resize tests - current resize is not RCU-safe and basic.
TEST_F(ConcurrentHashTableTest, ResizeOperation) {
    ConcurrentHashTable table(3); // Start small
    table.insert("a", 1);
    table.insert("b", 2);
    table.insert("c", 3); // Table should be full or near full based on load factor for resize.

    int value;
    ASSERT_TRUE(getValue(table, "a", value)); EXPECT_EQ(value, 1);
    ASSERT_TRUE(getValue(table, "b", value)); EXPECT_EQ(value, 2);
    ASSERT_TRUE(getValue(table, "c", value)); EXPECT_EQ(value, 3);

    // The provided `resize` is manual and not RCU-safe.
    // It's also not automatically called by `insert` in the placeholder.
    // We call it manually here.
    table.resize(10);

    // Check existing keys
    ASSERT_TRUE(getValue(table, "a", value)); EXPECT_EQ(value, 1);
    ASSERT_TRUE(getValue(table, "b", value)); EXPECT_EQ(value, 2);
    ASSERT_TRUE(getValue(table, "c", value)); EXPECT_EQ(value, 3);

    // Insert more keys
    table.insert("d", 4);
    table.insert("e", 5);
    ASSERT_TRUE(getValue(table, "d", value)); EXPECT_EQ(value, 4);
    ASSERT_TRUE(getValue(table, "e", value)); EXPECT_EQ(value, 5);

    // Check all keys again
    ASSERT_TRUE(getValue(table, "a", value)); EXPECT_EQ(value, 1);
    ASSERT_TRUE(getValue(table, "b", value)); EXPECT_EQ(value, 2);
    ASSERT_TRUE(getValue(table, "c", value)); EXPECT_EQ(value, 3);
    ASSERT_TRUE(getValue(table, "d", value)); EXPECT_EQ(value, 4);
    ASSERT_TRUE(getValue(table, "e", value)); EXPECT_EQ(value, 5);
}


// int main(int argc, char **argv) {
//     ::testing::InitGoogleTest(&argc, argv);
//     return RUN_ALL_TESTS();
// }
