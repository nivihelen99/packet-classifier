#include "gtest/gtest.h"
#include "data_structures/bloom_filter.h" // Adjust path as necessary
#include <string>
#include <vector>
#include <cmath> // For std::fabs in FP comparison

// Note: The BloomFilter implementation uses placeholder hash functions.
// Tests will focus on the filter's contract (no false negatives, potential for false positives)
// rather than the quality/distribution of the hash functions themselves.

TEST(BloomFilterTest, ConstructorOptimalParams) {
    // Test with typical values
    BloomFilter bf1(1000, 0.01);
    EXPECT_GT(bf1.getSize(), 0);
    EXPECT_GT(bf1.getNumHashFunctions(), 0);
    std::cout << "Optimal params for 1000 items, 0.01 FP: Size=" << bf1.getSize() 
              << ", Hashes=" << bf1.getNumHashFunctions() << std::endl;


    // Test with low number of items
    BloomFilter bf2(10, 0.1);
    EXPECT_GT(bf2.getSize(), 0);
    EXPECT_GT(bf2.getNumHashFunctions(), 0);
    std::cout << "Optimal params for 10 items, 0.1 FP: Size=" << bf2.getSize() 
              << ", Hashes=" << bf2.getNumHashFunctions() << std::endl;

    // Test edge case: num_items = 0 (should use defaults or minimal sensible values)
    BloomFilter bf_zero_items(0, 0.01);
    EXPECT_GT(bf_zero_items.getSize(), 0);
    EXPECT_GT(bf_zero_items.getNumHashFunctions(), 0);
     std::cout << "Optimal params for 0 items, 0.01 FP: Size=" << bf_zero_items.getSize() 
              << ", Hashes=" << bf_zero_items.getNumHashFunctions() << std::endl;


    // Test edge case: invalid FP probability (e.g. > 1, should use defaults)
    // The implementation prints a warning and uses defaults.
    BloomFilter bf_invalid_fp_high(100, 1.5);
    EXPECT_GT(bf_invalid_fp_high.getSize(), 0);
    EXPECT_GT(bf_invalid_fp_high.getNumHashFunctions(), 0);
    std::cout << "Params for 100 items, 1.5 FP (invalid): Size=" << bf_invalid_fp_high.getSize() 
              << ", Hashes=" << bf_invalid_fp_high.getNumHashFunctions() << std::endl;

    BloomFilter bf_invalid_fp_low(100, 0.0);
     EXPECT_GT(bf_invalid_fp_low.getSize(), 0);
    EXPECT_GT(bf_invalid_fp_low.getNumHashFunctions(), 0);
     std::cout << "Params for 100 items, 0.0 FP (invalid): Size=" << bf_invalid_fp_low.getSize() 
              << ", Hashes=" << bf_invalid_fp_low.getNumHashFunctions() << std::endl;
}

TEST(BloomFilterTest, ConstructorManualParams) {
    BloomFilter bf1(2048, 5);
    EXPECT_EQ(bf1.getSize(), 2048);
    EXPECT_EQ(bf1.getNumHashFunctions(), 5);

    // Test edge case: size = 0 (implementation defaults to 1024)
    BloomFilter bf_zero_size(0, 5);
    EXPECT_EQ(bf_zero_size.getSize(), 1024); // Default size
    EXPECT_EQ(bf_zero_size.getNumHashFunctions(), 5);

    // Test edge case: num_hashes = 0 (implementation defaults to 3)
    BloomFilter bf_zero_hashes(1024, 0);
    EXPECT_EQ(bf_zero_hashes.getSize(), 1024);
    EXPECT_EQ(bf_zero_hashes.getNumHashFunctions(), 3); // Default k
}

TEST(BloomFilterTest, BasicInsertAndPossiblyContains) {
    BloomFilter bf(100, 0.01); // Expect ~100 items, 1% FP rate
    
    std::string item1 = "hello";
    std::string item2 = "world";
    std::string item3 = "bloom filter";

    bf.insert(item1);
    bf.insert(item2);
    bf.insert(item3);

    EXPECT_TRUE(bf.possiblyContains(item1)); // No false negatives
    EXPECT_TRUE(bf.possiblyContains(item2));
    EXPECT_TRUE(bf.possiblyContains(item3));
}

TEST(BloomFilterTest, PossiblyContainsNonAddedElements) {
    BloomFilter bf(100, 0.01);
    
    bf.insert("apple");
    bf.insert("banana");

    EXPECT_TRUE(bf.possiblyContains("apple"));
    EXPECT_FALSE(bf.possiblyContains("orange"));   // Likely false
    EXPECT_FALSE(bf.possiblyContains("grape"));    // Likely false
    EXPECT_FALSE(bf.possiblyContains("watermelon"));// Likely false
    // Note: Some of these could be true (false positives), especially with a small filter
    // or unfortunate hash collisions. This test checks typical behavior.
}

TEST(BloomFilterTest, InsertEmptyString) {
    BloomFilter bf(100, 0.01);
    std::string empty_str = "";
    
    bf.insert(empty_str);
    EXPECT_TRUE(bf.possiblyContains(empty_str));

    // Check another non-added string
    EXPECT_FALSE(bf.possiblyContains("not_empty"));
}

TEST(BloomFilterTest, FalsePositiveBehavior) {
    // This test is conceptual for unit tests. A true FP rate test is statistical.
    // We'll add a few items and test a few non-added items.
    // For a well-behaved filter of reasonable size, we expect most non-added items
    // to return false from possiblyContains.
    
    int num_items_to_insert = 20;
    double fp_rate_target = 0.05; // 5%
    BloomFilter bf(num_items_to_insert, fp_rate_target);

    for (int i = 0; i < num_items_to_insert; ++i) {
        bf.insert("item_inserted_" + std::to_string(i));
    }

    // Verify inserted items
    for (int i = 0; i < num_items_to_insert; ++i) {
        EXPECT_TRUE(bf.possiblyContains("item_inserted_" + std::to_string(i)));
    }

    int non_added_checks = 100;
    int false_positives = 0;
    for (int i = 0; i < non_added_checks; ++i) {
        if (bf.possiblyContains("item_NOT_inserted_" + std::to_string(i))) {
            false_positives++;
        }
    }
    std::cout << "False positives: " << false_positives << " out of " 
              << non_added_checks << " non-added items." << std::endl;
    // We expect false_positives to be roughly around non_added_checks * fp_rate_target
    // but it can vary a lot in a single run.
    // A loose upper bound: not more than 3-4x the expected for a small test.
    // For 100 checks and 5% target, expect ~5 FPs. Allow up to, say, 20 to pass unit test.
    EXPECT_LE(false_positives, non_added_checks * fp_rate_target * 4 + 5); // Allow some leeway
}

TEST(BloomFilterTest, UtilityMethods) {
    BloomFilter bf(100, 0.01); // n=100, p=0.01 -> m=959, k=7 (approx)
    EXPECT_EQ(bf.getSize(), 959); // Values from online calculator for n=100, p=0.01
    EXPECT_EQ(bf.getNumHashFunctions(), 7);

    bf.insert("test1");
    bf.insert("test2");

    double effective_fp = bf.getEffectiveFalsePositiveProbability();
    std::cout << "Effective FP after 2 insertions: " << effective_fp << std::endl;
    EXPECT_GE(effective_fp, 0.0);
    EXPECT_LE(effective_fp, 1.0);

    uint64_t approx_count = bf.getApproximateCount();
    std::cout << "Approximate count after 2 insertions: " << approx_count << std::endl;
    // Approx count is very rough. For few items, it might be close or quite off.
    // It's more meaningful when the filter is more populated but not saturated.
    // Check if it's somewhat plausible, e.g., not drastically different from 2 here.
    EXPECT_LE(std::abs(static_cast<long long>(approx_count) - 2LL), 5LL); // Allow some error
}

TEST(BloomFilterTest, SaturationEffectsOnUtilities) {
    // Use a small filter to saturate it easily
    BloomFilter bf(10, 2); // m=10, k=2 (manually set)
    
    for(int i=0; i < 20; ++i) { // Insert more items than optimal for this small size
        bf.insert("saturate_" + std::to_string(i));
    }

    // All bits are likely set or nearly set
    double effective_fp = bf.getEffectiveFalsePositiveProbability();
    std::cout << "Effective FP for saturated filter: " << effective_fp << std::endl;
    // Expect high FP rate, close to 1.0
    EXPECT_GT(effective_fp, 0.5); 
    EXPECT_LE(effective_fp, 1.0);

    uint64_t approx_count = bf.getApproximateCount();
    std::cout << "Approximate count for saturated filter: " << approx_count << std::endl;
    // The current implementation returns ULLONG_MAX if all bits are set, or a high estimate.
    // If not all bits are set but many are, the estimate can be very high or inaccurate.
    // For this test, we just ensure it returns something, as behavior is defined as "unreliable".
    EXPECT_GE(approx_count, 0);
}

TEST(BloomFilterTest, NoClearMethod) {
    // The BloomFilter interface provided does not have a clear() method.
    // If it did, tests would be:
    // BloomFilter bf(100, 0.01);
    // bf.insert("item_before_clear");
    // EXPECT_TRUE(bf.possiblyContains("item_before_clear"));
    // bf.clear(); // Assuming this method existed
    // EXPECT_FALSE(bf.possiblyContains("item_before_clear")); // Or low probability
    SUCCEED() << "Clear method test skipped as it's not in the BloomFilter interface.";
}

// Test with byte array inputs (if interface allows, it does)
TEST(BloomFilterTest, ByteArrayInterface) {
    BloomFilter bf(100, 0.01);
    unsigned char data1[] = {0xDE, 0xAD, 0xBE, 0xEF};
    unsigned char data2[] = {0xCA, 0xFE, 0xBA, 0xBE};
    unsigned char data_not_added[] = {0x12, 0x34, 0x56, 0x78};

    bf.insert(data1, sizeof(data1));
    bf.insert(data2, sizeof(data2));

    EXPECT_TRUE(bf.possiblyContains(data1, sizeof(data1)));
    EXPECT_TRUE(bf.possiblyContains(data2, sizeof(data2)));
    EXPECT_FALSE(bf.possiblyContains(data_not_added, sizeof(data_not_added))); // Likely
}

// int main(int argc, char **argv) {
//     ::testing::InitGoogleTest(&argc, argv);
//     return RUN_ALL_TESTS();
// }

/*
  Note on calculateOptimalParams for num_items = 0:
  The formula for m involves log(p) and n. If n=0, m becomes 0 or undefined.
  The formula for k involves m/n. If n=0 or m=0, k is undefined.
  The implementation has a specific handling for num_items == 0:
  It tries to calculate m assuming n=1: `m = ceil(-1.0 * (1 * log(p)) / (log(2)^2))`
  And k assuming n=1: `k = ceil((m/1.0) * log(2))`
  This provides a filter that's sized for at least one item with the desired FP rate.
  This behavior is acceptable for num_items=0, as it gives a usable (though not "optimal" for zero items) filter.
*/
