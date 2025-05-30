#include "gtest/gtest.h"
#include "data_structures/compressed_trie.h" // Adjust path as necessary

// Note: The CompressedTrie implementation is currently a placeholder
// using std::string for "IP prefixes" and char-based nodes.
// These tests are written against this placeholder implementation.
// They do not validate IP-specific logic (e.g., binary prefix matching,
// longest prefix match for actual IP addresses, CIDR notation handling)
// or any compression algorithms, as these are not yet implemented.

TEST(CompressedTriePlaceholderTest, EmptyTrie) {
    CompressedTrie trie;
    // Lookup in an empty trie should not find anything
    EXPECT_EQ(trie.lookup("any_string"), -1);
    EXPECT_EQ(trie.lookup(""), -1); // Test with empty string
}

TEST(CompressedTriePlaceholderTest, BasicInsertAndLookup) {
    CompressedTrie trie;
    trie.insert("apple", 10);
    trie.insert("apricot", 20);

    EXPECT_EQ(trie.lookup("apple"), 10);
    EXPECT_EQ(trie.lookup("apricot"), 20);   // Exact match

    // Test for lookups based on current "longest prefix" behavior
    EXPECT_EQ(trie.lookup("app"), -1);       // "app" itself is not marked as end_of_prefix
    EXPECT_EQ(trie.lookup("apples"), 10);    // "apple" is a prefix of "apples" and is end_of_prefix
    EXPECT_EQ(trie.lookup("applepie"), 10);  // "apple" is a prefix of "applepie"
    EXPECT_EQ(trie.lookup("apricots"), 20);  // "apricot" is a prefix of "apricots"
    
    EXPECT_EQ(trie.lookup("banana"), -1);    // No prefix matches
    EXPECT_EQ(trie.lookup(""), -1);          // Empty string lookup (root not marked initially)
}

TEST(CompressedTriePlaceholderTest, InsertPrefixOfExisting) {
    CompressedTrie trie;
    trie.insert("applepie", 100);
    EXPECT_EQ(trie.lookup("applepie"), 100);
    EXPECT_EQ(trie.lookup("apple"), -1); // "apple" is not yet an explicit key

    trie.insert("apple", 200);
    EXPECT_EQ(trie.lookup("apple"), 200); // Now "apple" is an explicit key
    EXPECT_EQ(trie.lookup("applepie"), 100); // Original key should still exist
}

TEST(CompressedTriePlaceholderTest, InsertOverExisting) {
    CompressedTrie trie;
    trie.insert("test", 5);
    EXPECT_EQ(trie.lookup("test"), 5);

    trie.insert("test", 15); // Re-inserting the same key, should update data
    EXPECT_EQ(trie.lookup("test"), 15);
}

TEST(CompressedTriePlaceholderTest, LookupReturnsLongestMatchFromStringPerspective) {
    CompressedTrie trie;
    trie.insert("a", 1);
    trie.insert("ab", 2);
    trie.insert("abc", 3);

    // Current placeholder lookup logic finds the data associated with the *last* node
    // in the path that is_end_of_prefix.
    // If "a", "ab", "abc" are all marked as end_of_prefix:
    // lookup("a") -> 1
    // lookup("ab") -> 2
    // lookup("abc") -> 3
    // lookup("abcd") -> 3 (matches "abc")
    // lookup("ax") -> 1 (matches "a")

    EXPECT_EQ(trie.lookup("a"), 1);
    EXPECT_EQ(trie.lookup("ab"), 2);
    EXPECT_EQ(trie.lookup("abc"), 3);
    EXPECT_EQ(trie.lookup("abcd"), 3); // Longest match is "abc"
    EXPECT_EQ(trie.lookup("ax"), 1);   // Longest match is "a"
    EXPECT_EQ(trie.lookup("b"), -1);
}

TEST(CompressedTriePlaceholderTest, BasicRemove) {
    CompressedTrie trie;
    trie.insert("one", 1);
    trie.insert("two", 2);
    trie.insert("three", 3);

    EXPECT_EQ(trie.lookup("one"), 1);
    EXPECT_EQ(trie.lookup("two"), 2);
    EXPECT_EQ(trie.lookup("three"), 3);

    trie.remove("two");
    EXPECT_EQ(trie.lookup("one"), 1);
    EXPECT_EQ(trie.lookup("two"), -1); // "two" should be gone
    EXPECT_EQ(trie.lookup("three"), 3);

    // Try removing a non-existent key
    trie.remove("four"); // Should not affect existing keys
    EXPECT_EQ(trie.lookup("one"), 1);
    EXPECT_EQ(trie.lookup("three"), 3);

    // Try removing a key that's a prefix of another, but not inserted itself
    trie.insert("prefix", 10);
    trie.insert("prefix_longer", 20);
    trie.remove("pre"); // "pre" was not inserted
    EXPECT_EQ(trie.lookup("prefix"), 10); // Should still exist
    EXPECT_EQ(trie.lookup("prefix_longer"), 20); // Should still exist
}

TEST(CompressedTriePlaceholderTest, RemoveMakesIntermediateNodeNotEndOfPrefix) {
    CompressedTrie trie;
    trie.insert("path", 10);
    trie.insert("pathway", 20);

    EXPECT_EQ(trie.lookup("path"), 10);
    EXPECT_EQ(trie.lookup("pathway"), 20);

    trie.remove("path");
    EXPECT_EQ(trie.lookup("path"), -1); // "path" itself is no longer a valid end
    EXPECT_EQ(trie.lookup("pathway"), 20); // "pathway" should still be lookup-able as its path exists

    // The placeholder remove doesn't actually prune nodes, just marks them.
    // So "pathw" would still match "path"s node if it were an end, then continue.
    // The current lookup logic would return data from "pathway" if we looked up "pathw"
    // because "pathway" is the next valid end_of_prefix along that path.
    // This is a bit specific to the current placeholder's lookup behavior.
}

TEST(CompressedTriePlaceholderTest, RemoveNonExistent) {
    CompressedTrie trie;
    trie.insert("key1", 1);
    trie.remove("key2"); // Remove a key that was never there
    trie.remove("");     // Remove empty string
    EXPECT_EQ(trie.lookup("key1"), 1);
}

TEST(CompressedTriePlaceholderTest, InsertEmptyString) {
    CompressedTrie trie;
    // The current placeholder trie uses char-by-char traversal.
    // Inserting an empty string means the root node itself is marked as end_of_prefix.
    trie.insert("", 123);
    EXPECT_EQ(trie.lookup(""), 123);
    EXPECT_EQ(trie.lookup("anything"), 123); // Current lookup finds root if it's a match

    trie.insert("next", 456);
    EXPECT_EQ(trie.lookup("next"), 456);
    EXPECT_EQ(trie.lookup(""), 123); // Root still a match
    EXPECT_EQ(trie.lookup("n"), 123); // 'n' will match root first, then proceed.
                                      // If root is end_of_prefix, it will be the longest match for "n"
                                      // unless "n" itself is also an end_of_prefix.

    trie.insert("n", 789);
    EXPECT_EQ(trie.lookup("n"), 789); // Now "n" is a more specific match.
    EXPECT_EQ(trie.lookup("ne"), 789); // Matches "n"
    EXPECT_EQ(trie.lookup(""), 123); // Root is still there
}

TEST(CompressedTriePlaceholderTest, RemoveEmptyString) {
    CompressedTrie trie;
    trie.insert("", 50);
    trie.insert("test", 60);

    EXPECT_EQ(trie.lookup(""), 50);
    EXPECT_EQ(trie.lookup("any"), 50); // Matches root
    EXPECT_EQ(trie.lookup("test"), 60);

    trie.remove("");
    EXPECT_EQ(trie.lookup(""), -1);
    EXPECT_EQ(trie.lookup("any"), -1); // Should no longer match root by default
    EXPECT_EQ(trie.lookup("test"), 60); // "test" should remain
}

// Main function for Google Test (needed if not linking with gtest_main)
// int main(int argc, char **argv) {
//   ::testing::InitGoogleTest(&argc, argv);
//   return RUN_ALL_TESTS();
// }
// This is not needed as we link with GTest::gtest_main which provides a main.

/*
    Further tests that would be relevant for a *real* IP Compressed Trie:
    - TestInsertionAndLookupIPV4:
        - "10.0.0.0/8", 1
        - "10.1.0.0/16", 2
        - "192.168.1.0/24", 3
        - Lookup "10.0.0.1" -> 1
        - Lookup "10.1.0.1" -> 2
        - Lookup "10.2.0.1" -> 1 (matches 10.0.0.0/8)
        - Lookup "192.168.1.100" -> 3
        - Lookup "172.16.0.1" -> -1 (or default route if inserted)
    - TestInsertionAndLookupIPV6: (similar, with IPv6 addresses)
    - TestDeleteIPV4:
        - Insert "10.0.0.0/8", 1
        - Insert "10.1.0.0/16", 2
        - Delete "10.1.0.0/16"
        - Lookup "10.1.0.1" -> 1 (should now match 10.0.0.0/8)
        - Delete "10.0.0.0/8"
        - Lookup "10.0.0.1" -> -1
    - TestDefaultRoute:
        - Insert "0.0.0.0/0", 100
        - Lookup "1.2.3.4" -> 100
        - Lookup "192.168.1.1" -> 100
        - Insert "192.168.1.0/24", 200
        - Lookup "192.168.1.1" -> 200 (more specific)
        - Lookup "1.2.3.4" -> 100
    - TestThirtyTwoBitPrefix:
        - Insert "192.168.1.1/32", 1
        - Lookup "192.168.1.1" -> 1
        - Lookup "192.168.1.2" -> -1
    - TestCompressionEffectiveness (if compression was implemented):
        - Insert many prefixes that allow for significant compression.
        - Check node count or structure depth before and after compression.
    - TestInvalidInputs:
        - trie.insert("10.0.0.0/33", 1) // Invalid prefix length
        - trie.insert("300.0.0.0/8", 1) // Invalid IP
        - Verify behavior (e.g., throws exception, returns error code)
*/
