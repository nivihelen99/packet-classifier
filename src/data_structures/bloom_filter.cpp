#include "data_structures/bloom_filter.h"
#include <iostream> // For placeholder output
#include <limits>   // For std::numeric_limits

// --- Helper for Optimal Parameters ---
void BloomFilter::calculateOptimalParams(uint64_t num_items, double false_positive_prob, uint64_t& out_size, int& out_num_hashes) {
    if (num_items == 0 || false_positive_prob <= 0.0 || false_positive_prob >= 1.0) {
        // Default or error case
        out_size = 1024; // Some default size
        out_num_hashes = 3;  // Some default k
        if (num_items > 0 && (false_positive_prob <= 0.0 || false_positive_prob >= 1.0)) {
             std::cerr << "Warning: Invalid false_positive_prob. Using default parameters." << std::endl;
        }
         if (num_items == 0 && false_positive_prob > 0.0 && false_positive_prob < 1.0) {
             std::cout << "Info: num_items is 0. Optimal size/hashes not strictly calculable, using defaults for given prob." << std::endl;
             // Or base on a minimum sensible size for the given probability
             out_size = static_cast<uint64_t>(std::ceil(-1.0 * (1 * std::log(false_positive_prob)) / (std::log(2) * std::log(2))));
             if (out_size == 0) out_size = 100; // Ensure non-zero size
             out_num_hashes = static_cast<int>(std::ceil((out_size / 1.0) * std::log(2))); // Assuming n=1 for this edge case to get k
             if (out_num_hashes == 0) out_num_hashes = 1;
             return;
        }
        return;
    }
    // m = - (n * ln(p)) / (ln(2)^2)
    out_size = static_cast<uint64_t>(std::ceil(-1.0 * (num_items * std::log(false_positive_prob)) / (std::log(2) * std::log(2))));
    // k = (m / n) * ln(2)
    out_num_hashes = static_cast<int>(std::ceil((static_cast<double>(out_size) / num_items) * std::log(2)));

    if (out_size == 0) out_size = 100; // Minimum sensible size
    if (out_num_hashes == 0) out_num_hashes = 1; // Minimum sensible k
    if (out_num_hashes > 16) out_num_hashes = 16; // Practical upper limit for k for this skeleton
}

// --- Constructors ---
BloomFilter::BloomFilter(uint64_t num_items, double false_positive_prob) : current_insertions(0) {
    calculateOptimalParams(num_items, false_positive_prob, this->bit_array_size, this->num_hash_functions);
    if (this->bit_array_size == 0) { // Ensure bit_array is not size 0
        std::cerr << "Warning: Calculated bit_array_size is 0. Defaulting to 1024." << std::endl;
        this->bit_array_size = 1024;
    }
    try {
        bit_array.resize(this->bit_array_size, false);
    } catch (const std::bad_alloc& e) {
        std::cerr << "Error: Failed to allocate bit_array of size " << this->bit_array_size << ". " << e.what() << std::endl;
        // Fallback or rethrow
        this->bit_array_size = 0;
        this->num_hash_functions = 0;
        throw; // Rethrow or handle more gracefully
    }
    std::cout << "BloomFilter initialized. Optimal size: " << this->bit_array_size
              << ", Optimal hash functions: " << this->num_hash_functions
              << " for " << num_items << " items and FP prob: " << false_positive_prob << std::endl;
}

BloomFilter::BloomFilter(uint64_t size, int num_hashes)
    : bit_array_size(size), num_hash_functions(num_hashes), current_insertions(0) {
    if (size == 0) {
        std::cerr << "Warning: BloomFilter size cannot be 0. Defaulting to 1024." << std::endl;
        this->bit_array_size = 1024;
    }
    if (num_hashes <= 0) {
        std::cerr << "Warning: BloomFilter num_hash_functions must be positive. Defaulting to 3." << std::endl;
        this->num_hash_functions = 3;
    }
     try {
        bit_array.resize(this->bit_array_size, false);
    } catch (const std::bad_alloc& e) {
        std::cerr << "Error: Failed to allocate bit_array of size " << this->bit_array_size << ". " << e.what() << std::endl;
        this->bit_array_size = 0; // Indicate failure
        this->num_hash_functions = 0;
        throw;
    }
    std::cout << "BloomFilter initialized with size: " << this->bit_array_size
              << " and hash functions: " << this->num_hash_functions << std::endl;
}

BloomFilter::~BloomFilter() {
    std::cout << "BloomFilter destroyed." << std::endl;
}

// --- Hashing ---
// These are placeholder hash functions. Real implementations should use good quality,
// independent hash functions like MurmurHash3, xxHash, or FNV.
// For simplicity, we'll use combinations/variations of std::hash.

uint64_t BloomFilter::hashFunction1(const unsigned char* data, size_t len) const {
    // Simple hash: sum of chars (very bad, just for placeholder)
    // In practice, use a robust hash like std::hash for string or a custom one.
    std::string s(reinterpret_cast<const char*>(data), len);
    return std::hash<std::string>{}(s);
}

uint64_t BloomFilter::hashFunction2(const unsigned char* data, size_t len) const {
    // Another simple hash (equally bad, just for placeholder)
    // Ensure this is different from hashFunction1.
    uint64_t hash_val = 5381; // DJB2 seed
    for (size_t i = 0; i < len; ++i) {
        hash_val = ((hash_val << 5) + hash_val) + data[i]; // hash * 33 + c
    }
    return hash_val;
}

// General hash generation (produces k hash values)
std::vector<uint64_t> BloomFilter::hash(const unsigned char* data, size_t len) const {
    std::vector<uint64_t> hashes;
    hashes.reserve(num_hash_functions);

    if (num_hash_functions == 0 || bit_array_size == 0) return hashes;

    // For a skeleton, let's assume we have at least two base hash functions
    // and derive others if k > 2.
    // This is a common technique: h_i(x) = (h1(x) + i * h2(x)) mod m
    // It's better than just using one hash and slicing bits, but still not as good as k truly independent hashes.
    uint64_t h1 = hashFunction1(data, len);
    uint64_t h2 = (num_hash_functions > 1) ? hashFunction2(data, len) : 0; // h2 only needed if k > 1

    for (int i = 0; i < num_hash_functions; ++i) {
        if (i == 0) {
            hashes.push_back(h1 % bit_array_size);
        } else if (i == 1 && num_hash_functions > 1) {
             hashes.push_back(h2 % bit_array_size);
        }
        else { // For i >= 2 (or i >= 1 if only one base hash was used e.g. h2 was 0)
            // Kirsch-Mitzenmacher optimization or simple linear combination
            // (h1 + i * h2) can lead to poor distribution if h2 is 0 or small.
            // Ensure h2 is non-zero and reasonably large if used this way.
            // If h2 could be 0 (e.g. if num_hash_functions was 1 and we are in the i=0 case of a loop for k),
            // then h1 + i*h2 would just be h1.
            // A better approach if only two base hashes h1, h2 are available:
            // hash_i = (h1 + i * (h2 + i)) % bit_array_size; // Ensure h2+i is not 0
            uint64_t combined_hash = (h1 + static_cast<uint64_t>(i) * (h2 + static_cast<uint64_t>(i) + 1)) % bit_array_size;
            hashes.push_back(combined_hash);
        }
    }
    return hashes;
}


// --- Core Public Methods ---
void BloomFilter::insert(const std::string& item) {
    insert(reinterpret_cast<const unsigned char*>(item.c_str()), item.length());
}

void BloomFilter::insert(const unsigned char* data, size_t len) {
    if (bit_array_size == 0) {
        std::cerr << "Error: Bloom filter not properly initialized (size 0). Cannot insert." << std::endl;
        return;
    }
    std::cout << "Inserting data of length " << len << "." << std::endl;
    std::vector<uint64_t> hash_values = hash(data, len);
    for (uint64_t h_val : hash_values) {
        bit_array[h_val] = true;
    }
    current_insertions++;
}

bool BloomFilter::possiblyContains(const std::string& item) const {
    return possiblyContains(reinterpret_cast<const unsigned char*>(item.c_str()), item.length());
}

bool BloomFilter::possiblyContains(const unsigned char* data, size_t len) const {
     if (bit_array_size == 0) {
        std::cerr << "Warning: Bloom filter not properly initialized (size 0). Returning false." << std::endl;
        return false; // Or throw error
    }
    std::cout << "Checking data of length " << len << "." << std::endl;
    std::vector<uint64_t> hash_values = hash(data, len);
    for (uint64_t h_val : hash_values) {
        if (!bit_array[h_val]) {
            return false; // Definitely not present
        }
    }
    return true; // Possibly present
}

// --- Utility Methods ---
double BloomFilter::getEffectiveFalsePositiveProbability() const {
    if (bit_array_size == 0) return 1.0; // Max FP if not initialized
    // p_effective = (1 - (1 - 1/m)^(kn))^k
    // More common approximation: (1 - e^(-kn/m))^k
    double kn_div_m = static_cast<double>(num_hash_functions * current_insertions) / bit_array_size;
    double prob_one_bit_still_zero = std::exp(-kn_div_m);
    double prob_all_bits_set = std::pow(1.0 - prob_one_bit_still_zero, num_hash_functions);
    return prob_all_bits_set;
}

// This is a very rough estimate for a standard Bloom filter.
// More advanced Bloom filter variants (like counting Bloom filters) can do this properly.
uint64_t BloomFilter::getApproximateCount() const {
    if (bit_array_size == 0 || num_hash_functions == 0) return 0;
    long double count_set_bits = 0;
    for (bool bit : bit_array) {
        if (bit) {
            count_set_bits++;
        }
    }
    // n* = - (m/k) * ln(1 - X/m)
    // where X is the number of set bits.
    if (count_set_bits == bit_array_size) {
         // If all bits are set, estimation is very unreliable, could be very high.
         // Return a value indicating it's full or use max_uint if that's the convention.
         std::cout << "Warning: Bloom filter is saturated. Count estimation is highly unreliable." << std::endl;
         return std::numeric_limits<uint64_t>::max(); // Or expected_items if known
    }
    if (count_set_bits == 0) return 0;

    double estimate = - (static_cast<double>(bit_array_size) / num_hash_functions) *
                      std::log(1.0 - (count_set_bits / static_cast<double>(bit_array_size)));
    
    if (estimate < 0) return current_insertions; // Should not happen with valid inputs to log
    return static_cast<uint64_t>(std::round(estimate));
}
