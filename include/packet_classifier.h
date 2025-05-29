#ifndef PACKET_CLASSIFIER_H
#define PACKET_CLASSIFIER_H

#include <string>
#include <vector>
#include <cstdint> // For uint32_t, uint16_t etc.
#include <map>
#include <memory> // For std::shared_ptr, std::unique_ptr

// Include Phase 1 Data Structures
#include "data_structures/compressed_trie.h"
#include "data_structures/concurrent_hash.h"
#include "data_structures/interval_tree.h"
#include "data_structures/bloom_filter.h"

// Include Phase 1 Utilities
#include "utils/memory_pool.h"
#include "utils/logging.h"
#include "utils/threading.h" // For ReadWriteLock
#include "utils/rule_manager.h" // For RuleManager

// --- Core Data Structures from docs/requirement_design.md ---

// Represents the relevant fields from a packet header for classification
struct PacketHeader {
    uint32_t source_ip;
    uint32_t dest_ip;
    uint16_t source_port;
    uint16_t dest_port;
    uint8_t protocol; // e.g., TCP, UDP, ICMP
    // Potentially other fields like MAC addresses, VLAN tags, etc.
    // For simplicity, keeping it IP/port focused for now.
    // std::string source_mac;
    // std::string dest_mac;

    // Constructor (example)
    PacketHeader(uint32_t sip, uint32_t dip, uint16_t sport, uint16_t dport, uint8_t proto)
        : source_ip(sip), dest_ip(dip), source_port(sport), dest_port(dport), protocol(proto) {}
    
    // For hashing or using as key in maps if needed (though classification uses individual fields)
    std::string toString() const; // For logging or debugging
};

// Defines a filter condition for a field (e.g., IP address, port range)
struct PacketFilter {
    // For IP prefixes (could be CIDR string or value/mask)
    std::string source_ip_prefix; // e.g., "192.168.1.0/24"
    std::string dest_ip_prefix;

    // For port ranges
    uint16_t source_port_low = 0, source_port_high = 0; // 0 means any if not set or use specific "any" value
    uint16_t dest_port_low = 0, dest_port_high = 0;

    // For exact protocol match
    uint8_t protocol = 0; // 0 means any protocol

    // Default constructor for "any" filter
    PacketFilter() = default;

    // Basic matching logic. More advanced prefix/range matching will be handled
    // by specialized data structures in PacketClassifier.
    // This method provides a straightforward filter check.
    inline bool matches(const PacketHeader& header) const {
        // Protocol check
        if (protocol != 0 && protocol != header.protocol) {
            return false;
        }

        // Port range checks
        // Source Port
        if (source_port_low != 0 || source_port_high != 0) { // If a source port filter is set
            if (header.source_port < source_port_low || header.source_port > source_port_high) {
                return false;
            }
        }
        // Destination Port
        if (dest_port_low != 0 || dest_port_high != 0) { // If a destination port filter is set
            if (header.dest_port < dest_port_low || header.dest_port > dest_port_high) {
                return false;
            }
        }

        // IP Address checks - Simplified for this direct match function.
        // Trie-based prefix matching is the primary mechanism in PacketClassifier.
        // This direct check might be useful for rules that specify exact IPs without prefixes,
        // or if used in a context before Trie lookup.
        // If prefixes are present, this basic check should ideally not be the sole validator.
        // For now, if a prefix is set, we assume the Trie will handle it, so this function might
        // return true here, or we could implement a very basic exact match if no prefix "/" is found.

        // Example: Simple exact IP match if no prefix is indicated by typical means (e.g. no "/" in string)
        // This is highly simplified. Proper IP parsing and comparison are needed.
        if (!source_ip_prefix.empty()) {
            // If source_ip_prefix is something like "1.2.3.4" (exact) and header.source_ip needs conversion
            // This part is complex to implement correctly here without proper IP parsing utilities.
            // For skeleton: Assume if source_ip_prefix is set, it's handled by Trie.
            // So, for PacketFilter::matches, we don't return false based on IP if prefixes are involved.
            // If it were an exact IP, we would compare.
            // For now, we'll skip IP matching here and assume specialized structures do it.
        }
        if (!dest_ip_prefix.empty()) {
            // Similarly, assume Trie handles this.
        }

        return true; // If all checks passed or were deferred to specialized structures
    }

    std::string toString() const; // For logging or debugging
};

// Actions to be taken if a packet matches a rule
struct ActionList {
    enum class ActionType {
        FORWARD,    // Forward to a specific next_hop/interface
        DROP,       // Drop the packet
        LOG,        // Log the packet
        MIRROR,     // Mirror to a specific destination
        // ... other actions
    };

    ActionType primary_action = ActionType::DROP; // Default action
    int next_hop_id = -1; // Relevant for FORWARD action
    std::string log_identifier; // Relevant for LOG action
    // Other action-specific parameters

    std::string toString() const; // For logging or debugging
};

// A classification rule: combines filters with actions and metadata
struct ClassificationRule {
    int rule_id;        // Unique identifier for the rule
    int priority;       // Higher value means higher priority
    PacketFilter filter;
    ActionList actions;
    bool enabled = true; // Rule can be disabled

    // For statistics
    uint64_t match_count = 0;
    uint64_t last_match_time = 0; // Timestamp

    ClassificationRule(int id, int prio, const PacketFilter& f, const ActionList& a)
        : rule_id(id), priority(prio), filter(f), actions(a) {}
    
    std::string toString() const; // For logging or debugging
};

// Result of a classification lookup
struct ClassificationResult {
    bool matched = false;
    int matched_rule_id = -1; // ID of the rule that matched
    ActionList actions;       // Actions to take for the matched packet
    // Potentially, pointer to the matched rule itself for more info

    std::string toString() const; // For logging or debugging
};

// --- PacketClassifier Class ---
class PacketClassifier {
public:
    PacketClassifier(bool enable_bloom_filter_optimization = true);
    ~PacketClassifier();

    // --- Rule Management API ---
    // Returns true on success, false on failure (e.g., rule_id exists, invalid rule)
    bool addRule(const ClassificationRule& rule);
    bool deleteRule(int rule_id);
    bool modifyRule(int rule_id, const ClassificationRule& new_rule_content); // Can modify filter, actions, priority, enabled status

    // --- Classification API ---
    ClassificationResult classify(const PacketHeader& header);
    std::vector<ClassificationResult> classifyBatch(const std::vector<PacketHeader>& headers);

    // --- Statistics API ---
    std::map<int, uint64_t> getStatistics() const; // Returns map of rule_id to match_count
    uint64_t getRuleStatistics(int rule_id) const;
    void resetStatistics();
    void resetRuleStatistics(int rule_id);


private:
    // --- Internal Data Structures ---
    // Primary storage for rules, perhaps sorted by priority or in a structure that allows efficient iteration.
    // A std::map<int, ClassificationRule> could store rules by ID for quick lookup for delete/modify.
    // A std::vector<ClassificationRule> (or list of shared_ptrs) sorted by priority for classification.
    // std::map<int, ClassificationRule> rules_by_id_; // Moved to RuleManager
    // std::vector<ClassificationRule*> rules_by_priority_; // Moved to RuleManager

    // Specialized data structures for matching specific fields:
    // These would store references/IDs to rules. The rules themselves are managed by RuleManager.
    std::unique_ptr<CompressedTrie> source_ip_trie_;    // For source IP prefix matching
    std::unique_ptr<CompressedTrie> dest_ip_trie_;      // For destination IP prefix matching
    
    // For exact matches (e.g., full IP, MAC, or specific protocol if not handled by other means)
    // std::unique_ptr<ConcurrentHashTable> exact_match_table_; // Example if needed

    // For range matches (e.g., port numbers)
    std::unique_ptr<IntervalTree> source_port_tree_;    // For source port range matching
    std::unique_ptr<IntervalTree> dest_port_tree_;      // For destination port range matching

    // Optional: Bloom filter for fast rejection of non-matching packets
    std::unique_ptr<BloomFilter> bloom_filter_;
    bool use_bloom_filter_;

    // For managing memory for rule objects or other internal structures if not using standard containers directly
    // std::unique_ptr<MemoryPool> rule_memory_pool_; // If rules are numerous and fixed-size like

    // Rule manager instance
    std::unique_ptr<RuleManager> rule_manager_;

    // Logger instance
    Logger& logger_;
    
    // Lock for protecting specialized data structures (Tries, IntervalTrees) 
    // during updates by PacketClassifier itself. RuleManager has its own internal lock.
    ReadWriteLock specialized_structures_lock_;

    // --- Private Helper Methods ---
    // These methods will now operate on rule data obtained from the RuleManager.
    // They are responsible for updating the Tries, IntervalTrees, BloomFilter based on rule changes.
    bool updateSpecializedStructuresForRule(const ClassificationRule& rule); // Called on add or modify
    bool removeRuleFromSpecializedStructures(int rule_id); // Called on delete

    // Helper to convert string IP prefix to a format usable by CompressedTrie (e.g., bit string or uint/mask)
    // These are placeholders for actual IP parsing logic.
    // static std::string ipPrefixToBitString(const std::string& ip_prefix); 
    // static uint32_t ipStringToUint32(const std::string& ip_str); // e.g. "192.168.1.1" -> numeric
};

#endif // PACKET_CLASSIFIER_H
