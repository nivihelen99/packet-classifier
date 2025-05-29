#ifndef BLOOM_FILTER_H
#define BLOOM_FILTER_H

#include <vector>
#include <string>
#include <functional> // For std::hash and potentially other hash functions
#include <cmath>      // For log, pow
#include <cstdint>    // For uint64_t

// For hashing arbitrary data, we might need a way to get bytes
// For simplicity, operations will take const unsigned char* and size.
// Or, we can rely on std::string and std::hash specialization.

class BloomFilter {
public:
    // Constructor:
    // num_items: Expected number of items to be inserted.
    // false_positive_prob: Desired false positive probability.
    BloomFilter(uint64_t num_items, double false_positive_prob);

    // Constructor allowing manual specification of size and hash functions
    // size: Size of the bit array.
    // num_hashes: Number of hash functions to use.
    BloomFilter(uint64_t size, int num_hashes);

    ~BloomFilter();

    // Add an item to the Bloom filter.
    // Item is represented as a string for simplicity here.
    void insert(const std::string& item);
    void insert(const unsigned char* data, size_t len);

    // Check if an item might be in the set.
    // Returns true if the item is possibly in the set (could be a false positive).
    // Returns false if the item is definitely not in the set.
    bool possiblyContains(const std::string& item) const;
    bool possiblyContains(const unsigned char* data, size_t len) const;

    // --- Configuration & Utility ---
    uint64_t getSize() const { return bit_array_size; }
    int getNumHashFunctions() const { return num_hash_functions; }
    double getEffectiveFalsePositiveProbability() const; // Calculates based on current state
    uint64_t getApproximateCount() const; // Estimate number of items inserted (advanced)


private:
    uint64_t bit_array_size; // m
    int num_hash_functions;  // k
    std::vector<bool> bit_array;
    uint64_t current_insertions; // n (actual number of items inserted)

    // Helper to calculate optimal size and hash functions
    static void calculateOptimalParams(uint64_t num_items, double false_positive_prob, uint64_t& out_size, int& out_num_hashes);

    // Multiple hash functions.
    // These will produce k hash values for a given item.
    // Each hash value should be in the range [0, bit_array_size - 1].
    std::vector<uint64_t> hash(const unsigned char* data, size_t len) const;

    // Example internal hash functions (could use well-known non-cryptographic hashes like Murmur3, xxHash)
    // For this skeleton, we can use combinations of std::hash or simple custom ones.
    // It's crucial these are independent and uniformly distributed.
    uint64_t hashFunction1(const unsigned char* data, size_t len) const;
    uint64_t hashFunction2(const unsigned char* data, size_t len) const;
    // ... more hash functions up to num_hash_functions
};

#endif // BLOOM_FILTER_H
