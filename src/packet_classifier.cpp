#include "packet_classifier.h"
#include <algorithm> // For std::sort, std::remove_if, std::find_if
#include <iostream>  // For placeholder output in skeletons

// --- Helper toString() methods for core data structures ---
// PacketHeader::toString() is in the header (if simple enough) or here.
// Assuming it's defined in the header or a common utility place if complex.
// For PacketFilter::matches, it's now inline in the header.
// PacketFilter::toString() can remain here.

std::string PacketHeader::toString() const {
    // Implementation should be here if not inline or elsewhere
    std::stringstream ss;
    // Convert uint32_t IP to string format for logging if desired, or just log the uint32_t.
    // For now, just raw values.
    ss << "SrcIP: " << source_ip << ", DstIP: " << dest_ip
       << ", SrcPort: " << source_port << ", DstPort: " << dest_port
       << ", Proto: " << static_cast<int>(protocol);
    return ss.str();
}

std::string PacketFilter::toString() const {
    std::stringstream ss;
    ss << "SrcIP_Pfx: " << (source_ip_prefix.empty() ? "any" : source_ip_prefix)
       << ", DstIP_Pfx: " << (dest_ip_prefix.empty() ? "any" : dest_ip_prefix)
       << ", SrcPort: " << (source_port_low == 0 && source_port_high == 0 ? "any" : std::to_string(source_port_low) + "-" + std::to_string(source_port_high))
       << ", DstPort: " << (dest_port_low == 0 && dest_port_high == 0 ? "any" : std::to_string(dest_port_low) + "-" + std::to_string(dest_port_high))
       << ", Proto: " << (protocol == 0 ? "any" : std::to_string(static_cast<int>(protocol)));
    return ss.str();
}

std::string ActionList::toString() const {
    std::stringstream ss;
    switch (primary_action) {
        case ActionType::FORWARD: ss << "FORWARD (NextHop: " << next_hop_id << ")"; break;
        case ActionType::DROP:    ss << "DROP"; break;
        case ActionType::LOG:     ss << "LOG (ID: " << log_identifier << ")"; break;
        case ActionType::MIRROR:  ss << "MIRROR"; break;
        default:                  ss << "UNKNOWN_ACTION"; break;
    }
    return ss.str();
}

std::string ClassificationRule::toString() const {
    std::stringstream ss;
    ss << "RuleID: " << rule_id << ", Prio: " << priority << ", Enabled: " << (enabled ? "Yes" : "No")
       << "\n  Filter: " << filter.toString()
       << "\n  Actions: " << actions.toString()
       << "\n  Matches: " << match_count << ", LastMatch: " << last_match_time;
    return ss.str();
}

std::string ClassificationResult::toString() const {
    if (!matched) return "No Match";
    std::stringstream ss;
    ss << "Matched RuleID: " << matched_rule_id << "\n  Actions: " << actions.toString();
    return ss.str();
}


// --- PacketClassifier Implementation ---
PacketClassifier::PacketClassifier(bool enable_bloom_filter_optimization)
    : use_bloom_filter_(enable_bloom_filter_optimization),
      rule_manager_(std::make_unique<RuleManager>()), // Initialize RuleManager
      logger_(Logger::getInstance()) {

    logger_.info("PacketClassifier: Initializing...");

    // Initialize specialized data structures
    // The parameters (e.g., expected items, FP rate for BloomFilter) should be configurable.
    source_ip_trie_ = std::make_unique<CompressedTrie>();
    dest_ip_trie_ = std::make_unique<CompressedTrie>();
    
    source_port_tree_ = std::make_unique<IntervalTree>();
    dest_port_tree_ = std::make_unique<IntervalTree>();

    if (use_bloom_filter_) {
        // These values (10000 items, 0.01 FP rate) are placeholders.
        // They should be configured based on expected rule set size and desired performance.
        bloom_filter_ = std::make_unique<BloomFilter>(10000, 0.01);
        logger_.info("PacketClassifier: Bloom filter optimization enabled.");
    } else {
        logger_.info("PacketClassifier: Bloom filter optimization disabled.");
    }
    
    // exact_match_table_ = std::make_unique<ConcurrentHashTable>(); // If using
    // rule_memory_pool_ = std::make_unique<MemoryPool>(sizeof(ClassificationRule), 1024); // If using

    logger_.info("PacketClassifier: Initialization complete.");
}

PacketClassifier::~PacketClassifier() {
    logger_.info("PacketClassifier: Shutting down...");
    // std::unique_ptr members will be automatically deallocated.
}

// --- Rule Management API ---
bool PacketClassifier::addRule(const ClassificationRule& rule) {
    logger_.debug("PacketClassifier: Add rule ID: " + std::to_string(rule.rule_id) + " requested.");
    if (!rule_manager_->addRule(rule)) {
        // RuleManager already logged the specific error (e.g. duplicate, conflict)
        return false;
    }

    // If RuleManager added it successfully, update specialized structures
    {
        WriteLockGuard spec_lock(specialized_structures_lock_);
        if (!updateSpecializedStructuresForRule(rule.rule_id)) {
            // This is a potential inconsistency state. Specialized structure update failed.
            // Rollback rule addition in RuleManager? Or mark rule as inactive?
            // For now, log error. A robust system needs a clear strategy here.
            logger_.error("PacketClassifier: Rule ID " + std::to_string(rule.rule_id) + 
                          " added to RuleManager, but failed to update specialized structures. Potential inconsistency.");
            // Consider attempting to delete the rule from rule_manager_ here as a rollback.
            // rule_manager_->deleteRule(rule.rule_id); // Requires deleteRule to not fail if called like this
            return false; // Indicate overall failure
        }

        if (use_bloom_filter_) {
            const ClassificationRule* added_rule = rule_manager_->getRule(rule.rule_id);
            if (added_rule && added_rule->enabled) {
                 bloom_filter_->insert(added_rule->filter.toString());
            }
        }
    }
    logger_.info("PacketClassifier: Rule ID " + std::to_string(rule.rule_id) + " processed successfully.");
    return true;
}

bool PacketClassifier::deleteRule(int rule_id) {
    logger_.debug("PacketClassifier: Delete rule ID: " + std::to_string(rule_id) + " requested.");

    // First, attempt to remove from specialized structures.
    // This order (specialized first, then RuleManager) might be safer to avoid
    // having a rule in RuleManager that isn't in specialized structures if this part fails.
    {
        WriteLockGuard spec_lock(specialized_structures_lock_);
        if (!removeRuleFromSpecializedStructures(rule_id)) {
            // Log warning, but proceed to try to remove from RuleManager anyway,
            // as the rule might not have been in specialized structures for some reason.
            logger_.warning("PacketClassifier: Failed to remove rule ID " + std::to_string(rule_id) + 
                            " from specialized structures, or rule was not found there. Proceeding to RuleManager deletion.");
        }
        // Note: Bloom filter items are not removed in this skeleton.
    }

    if (!rule_manager_->deleteRule(rule_id)) {
        // RuleManager already logged the specific error (e.g. not found)
        // If specialized structure removal succeeded but this failed, it's an inconsistency.
        // However, if rule wasn't in specialized structures, failing here is okay.
        return false;
    }

    logger_.info("PacketClassifier: Rule ID " + std::to_string(rule_id) + " deleted successfully.");
    return true;
}

bool PacketClassifier::modifyRule(int rule_id, const ClassificationRule& new_rule_data) {
    logger_.debug("PacketClassifier: Modify rule ID: " + std::to_string(rule_id) + " requested.");

    // Get old rule data for Bloom Filter comparison if needed, before RuleManager updates it.
    // However, direct access to old rule before RuleManager's lock is tricky.
    // It's better to let RuleManager handle the update first.

    if (!rule_manager_->modifyRule(rule_id, new_rule_data)) {
        // RuleManager already logged.
        return false;
    }

    // RuleManager successfully modified it. Now update specialized structures.
    {
        WriteLockGuard spec_lock(specialized_structures_lock_);
        // Order: remove old representation, then add new representation.
        if (!removeRuleFromSpecializedStructures(rule_id)) {
             logger_.warning("PacketClassifier: Could not remove old state of modified rule ID " + std::to_string(rule_id) + 
                             " from specialized structures (may not have been there or error).");
        }
        if (!updateSpecializedStructuresForRule(rule_id)) {
            logger_.error("PacketClassifier: Rule ID " + std::to_string(rule_id) + 
                          " modified in RuleManager, but failed to update specialized structures. Potential inconsistency.");
            // Potential rollback or error state needed.
            return false;
        }

        if (use_bloom_filter_) {
            const ClassificationRule* modified_rule = rule_manager_->getRule(rule_id);
            if (modified_rule && modified_rule->enabled) {
                // Add new representation. Old one is not explicitly removed from Bloom Filter.
                bloom_filter_->insert(modified_rule->filter.toString());
            }
        }
    }
    logger_.info("PacketClassifier: Rule ID " + std::to_string(rule_id) + " modified successfully.");
    return true;
}


// --- Classification API ---
ClassificationResult PacketClassifier::classify(const PacketHeader& header) {
    // No top-level lock here for rule access; RuleManager's getRulesByPriority() provides a snapshot.
    // The specialized_structures_lock_ (read mode) would be needed if Tries/IntervalTrees are accessed directly here
    // AND if their internal operations are not independently thread-safe for reads.
    // CompressedTrie, IntervalTree skeletons currently don't show internal locking.
    // For now, assume their lookup methods are thread-safe for concurrent reads.
    // If not, a RcuUtils::ReadLockGuard(specialized_structures_lock_) would be needed here.
    ReadLockGuard spec_structures_read_lock(specialized_structures_lock_);

    logger_.trace("PacketClassifier: Classifying packet: " + header.toString());
    ClassificationResult result;
    if (use_bloom_filter_) {
        // Use the packet's string representation for the Bloom filter check.
        std::string packet_representation = header.toString();
        if (!bloom_filter_->possiblyContains(packet_representation)) {
            logger_.trace("PacketClassifier: Packet might be rejected by Bloom filter (possiblyContains returned false).");
            // If bloom_filter_->possiblyContains returns false, it means the item is *definitely not* in the set.
            // For packet classification, if the Bloom filter is used to store representations of *packets* that
            // are *known* to match some rule (less common), then a 'false' here means "definitely no rule for this exact packet flow".
            // More commonly, a Bloom filter might store elements of *rules*. In that case, a 'false' for a packet
            // characteristic might mean "no rule contains this characteristic".
            // The current simplistic usage (inserting rule.filter.toString() and packet.toString())
            // means it's checking if an identical filter string (for a rule) matches an identical packet string.
            // This is unlikely to be effective.
            // For this skeleton, we'll log it but proceed, as the Bloom filter strategy needs refinement.
        }
    }

    // Get rules sorted by priority from RuleManager
    std::vector<const ClassificationRule*> rules = rule_manager_->getRulesByPriority();
            // If bloom_filter_->possiblyContains returns false, it means the item is *definitely not* in the set.
            // For packet classification, this means no rule *likely* matches this exact packet's representation.
            // This type of Bloom filter usage is more for "is this specific flow known?"
            // For general rule matching, the Bloom filter would need to be constructed differently,
            // e.g., by adding all possible packet n-tuples that could match any rule. This is complex.
            // Alternative: use Bloom filter to check if parts of the packet (e.g. src_ip) are in any rule.

    // Iterate through rules (sorted by priority)
    for (const ClassificationRule* rule : rules) {
        if (!rule || !rule->enabled) continue; 

        // **Placeholder for specialized data structure lookups**
        // A more optimized approach would involve:
        // 1. Performing lookups in Tries/IntervalTrees based on packet header fields.
        //    These lookups would return a set of candidate rule IDs.
        //    Example: 
        //      candidate_rules_sip = source_ip_trie_->lookup(header.source_ip_as_string_or_bits);
        //      candidate_rules_dip = dest_ip_trie_->lookup(header.dest_ip_as_string_or_bits);
        //      candidate_rules_sport = source_port_tree_->findOverlappingIntervals(header.source_port);
        //      candidate_rules_dport = dest_port_tree_->findOverlappingIntervals(header.dest_port);
        //
        // 2. Intersecting these sets of candidate rule IDs to find rules that match on multiple fields.
        // 
        // 3. For rules that are candidates after specialized lookups, then proceed with
        //    the finer-grained `rule->filter.matches(header)` for any remaining checks
        //    not covered by the specialized structures (e.g., exact protocol if not in a specialized table).

        // Current skeleton: directly call rule->filter.matches() for each rule.
        // This relies on PacketFilter::matches() to do its part (protocol, ports).
        // IP prefix matching is assumed to be implicitly handled if this rule was chosen
        // by a (yet-to-be-implemented) specialized lookup phase.
        // For now, filter.matches() is simplified and doesn't do IP prefix checks itself.
        
        if (rule->filter.matches(header)) {
            // Conceptual: At this point, if we had a preceding specialized lookup phase,
            // this rule would be one of the candidates. The call to rule->filter.matches()
            // would then be a final validation or for fields not covered by specialized structures.

            // If PacketFilter::matches is the *only* check (as in current simple skeleton),
            // it needs to be comprehensive enough for correctness, though it won't be performant
            // for all field types (like IP prefixes).

            result.matched = true;
            result.matched_rule_id = rule->rule_id;
            result.actions = rule->actions;
            
            auto now = std::chrono::system_clock::now();
            auto epoch_time = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
            if (rule_manager_->incrementRuleMatchCount(rule->rule_id, epoch_time)) {
                // Successfully incremented
            } else {
                logger_.warning("PacketClassifier: Failed to increment match count for rule ID " + std::to_string(rule->rule_id));
            }
            
            logger_.debug("PacketClassifier: Packet matched rule ID " + std::to_string(rule->rule_id) + " using basic filter.matches().");
            return result; // First match (highest priority) wins
        }
    }

    logger_.trace("PacketClassifier: Packet did not match any enabled rules after iterating all.");
    // Define a default action if no rule matches.
    // For example, ActionType::DROP or a default FORWARD rule (not implemented here).
    result.matched = false;
    result.matched_rule_id = -1; // Or some indicator for "no match"
    // result.actions.primary_action = ActionList::ActionType::DROP; // Example default
    logger_.debug("PacketClassifier: No explicit rule matched. Applying default action (if any defined, otherwise 'no match').");

    return result; 
}

std::vector<ClassificationResult> PacketClassifier::classifyBatch(const std::vector<PacketHeader>& headers) {
    // No top-level PacketClassifier lock here for rule access.
    // Each call to classify() will get a fresh snapshot if needed, or RuleManager handles it.
    std::vector<ClassificationResult> results;
    results.reserve(headers.size());
    logger_.debug("PacketClassifier: Classifying batch of " + std::to_string(headers.size()) + " packets.");
    for (const auto& header : headers) {
        // In a batch, can optimize by not re-evaluating common parts for each packet if patterns exist.
        // For this skeleton, just call single classify.
        results.push_back(classify(header));
    }
    return results;
}

// --- Statistics API ---
std::map<int, uint64_t> PacketClassifier::getStatistics() const {
    //RcuUtils::ReadLockGuard r_lock(rule_management_lock_); // No longer needed, RuleManager handles its own locking
    logger_.debug("PacketClassifier: Retrieving all rule statistics.");
    std::map<int, uint64_t> stats;
    std::map<int, const ClassificationRule*> all_rules = rule_manager_->getAllRules();
    for (const auto& pair : all_rules) {
        if (pair.second) { // Check if rule pointer is not null
            stats[pair.first] = pair.second->match_count;
        }
    }
    return stats;
}

uint64_t PacketClassifier::getRuleStatistics(int rule_id) const {
    //RcuUtils::ReadLockGuard r_lock(rule_management_lock_); // No longer needed
    logger_.debug("PacketClassifier: Retrieving statistics for rule ID: " + std::to_string(rule_id));
    const ClassificationRule* rule = rule_manager_->getRule(rule_id);
    if (rule) {
        return rule->match_count;
    }
    logger_.warning("PacketClassifier: Rule ID " + std::to_string(rule_id) + " not found for statistics.");
    return 0;
}

void PacketClassifier::resetStatistics() {
    //RcuUtils::WriteLockGuard w_lock(rule_management_lock_); // No longer needed
    logger_.info("PacketClassifier: Resetting all rule statistics requested.");
    rule_manager_->resetAllRuleStatistics();
}

void PacketClassifier::resetRuleStatistics(int rule_id) {
    //RcuUtils::WriteLockGuard w_lock(rule_management_lock_); // No longer needed
    logger_.info("PacketClassifier: Resetting statistics for rule ID: " + std::to_string(rule_id) + " requested.");
    rule_manager_->resetRuleStatistics(rule_id);
}


// --- Private Helper Methods ---
// Note: These helpers now operate with rule_id and fetch rule data from RuleManager.
// They also need to use specialized_structures_lock_ for their operations.

bool PacketClassifier::updateSpecializedStructuresForRule(int rule_id) {
    WriteLockGuard spec_lock(specialized_structures_lock_);
    const ClassificationRule* rule = rule_manager_->getRule(rule_id);
    if (!rule) {
        logger_.error("PacketClassifier: Cannot update specialized structures. Rule ID " + std::to_string(rule_id) + " not found in RuleManager.");
        return false;
    }
    logger_.trace("PacketClassifier: Updating specialized structures for rule ID: " + std::to_string(rule->rule_id));
    
    // Placeholder for complex logic.
    // Example for source IP prefix:
    if (rule->enabled && !rule->filter.source_ip_prefix.empty()) {
        // Conceptual: source_ip_trie_->insert(parsed_prefix, rule->rule_id);
        // Where parsed_prefix is derived from rule->filter.source_ip_prefix.
        // The trie should associate this prefix with rule->rule_id.
        logger_.debug("Conceptual: Add src_ip_prefix '" + rule->filter.source_ip_prefix + "' from rule " + std::to_string(rule->rule_id) + " to source_ip_trie_.");
    } else if (!rule->enabled && !rule->filter.source_ip_prefix.empty()) {
        // If a rule is being updated to 'disabled', its components should be removed from matching structures.
        // This logic is better handled by removeRuleFromSpecializedStructures, then calling update.
        // For now: if disabled, we don't add. If it was enabled before, it should have been removed.
        logger_.debug("Conceptual: Rule " + std::to_string(rule->rule_id) + " is disabled. Ensure src_ip_prefix '" + rule->filter.source_ip_prefix + "' is NOT active in source_ip_trie_ for this rule.");
        // Conceptual: source_ip_trie_->remove(parsed_prefix, rule->rule_id);
    }
    // Similar for dest_ip_trie_

    // Example for source port range:
    if (rule->enabled && (rule->filter.source_port_low != 0 || rule->filter.source_port_high != 0)) {
        // Conceptual: source_port_tree_->insert(rule->filter.source_port_low, rule->filter.source_port_high, rule->rule_id);
        logger_.debug("Conceptual: Add src_port_range [" + std::to_string(rule->filter.source_port_low) + "-" + std::to_string(rule->filter.source_port_high) + "] from rule " + std::to_string(rule->rule_id) + " to source_port_tree_.");
    } else if (!rule->enabled && (rule->filter.source_port_low != 0 || rule->filter.source_port_high != 0)) {
        logger_.debug("Conceptual: Rule " + std::to_string(rule->rule_id) + " is disabled. Ensure src_port_range is NOT active in source_port_tree_ for this rule.");
        // Conceptual: source_port_tree_->remove(rule->filter.source_port_low, rule->filter.source_port_high, rule->rule_id);
    }
    // Similar for dest_port_tree_

    // Example for Bloom Filter update when a rule is added/enabled
    if (rule->enabled && use_bloom_filter_) {
        // Add a representation of the rule to the Bloom filter.
        // This helps in pre-filtering packets if the Bloom filter is queried correctly.
        // The string representation needs to be consistent and capture the essence of the filter.
        bloom_filter_->insert(rule->filter.toString()); 
        logger_.debug("Conceptual: Updated Bloom Filter for enabled rule ID " + std::to_string(rule->rule_id));
    }
    // Note: If a rule is disabled, removing its specific elements from a standard Bloom filter is not possible.
    // The filter would need to be rebuilt or be a Counting Bloom Filter.

    return true;
}

bool PacketClassifier::removeRuleFromSpecializedStructures(int rule_id) {
    WriteLockGuard spec_lock(specialized_structures_lock_);
    
    // Fetch the rule details from RuleManager to know what to remove.
    // This is important because RuleManager is the source of truth for rule definitions.
    // If the rule is already deleted from RuleManager (e.g. in deleteRule flow),
    // then this function might receive a rule_id for which getRule returns nullptr.
    // In such a case, we might not have the filter details.
    // The specialized data structures should ideally support removal by just rule_id if possible
    // (e.g., by scanning or having reverse maps, though less efficient).
    const ClassificationRule* rule = rule_manager_->getRule(rule_id); 
                                       // ^ This might be called *after* rule is deleted from manager.
                                       // This is a design consideration.
                                       // For modify: rule still exists. For delete: rule might be gone.
                                       // Let's assume for delete, we might need rule data passed in, or specialized structures handle rule_id only.

    if (!rule) {
         logger_.warning("PacketClassifier: Rule ID " + std::to_string(rule_id) + " not found in RuleManager during removeRuleFromSpecializedStructures. Attempting removal by ID only if structures support it.");
         // Conceptual: If tries/trees support remove_by_rule_id(rule_id)
         // source_ip_trie_->remove_associated_rule(rule_id);
         // dest_ip_trie_->remove_associated_rule(rule_id);
         // source_port_tree_->remove_associated_rule(rule_id);
         // dest_port_tree_->remove_associated_rule(rule_id);
         logger_.debug("Conceptual: Attempting to remove rule ID " + std::to_string(rule_id) + " from all specialized structures by ID only.");
         return true; // Assuming this operation is attempted.
    }

    logger_.trace("PacketClassifier: Removing rule ID: " + std::to_string(rule_id) + " (Filter: " + rule->filter.toString() + ") from specialized structures.");
    
    // Example for source IP prefix:
    if (!rule->filter.source_ip_prefix.empty()) {
        // Conceptual: source_ip_trie_->remove(parsed_prefix, rule_id);
        logger_.debug("Conceptual: Remove src_ip_prefix '" + rule->filter.source_ip_prefix + "' for rule " + std::to_string(rule_id) + " from source_ip_trie_.");
    }
    // Similar for dest_ip_trie_

    // Example for source port range:
    if (rule->filter.source_port_low != 0 || rule->filter.source_port_high != 0) {
        // Conceptual: source_port_tree_->remove(rule->filter.source_port_low, rule->filter.source_port_high, rule->rule_id);
        logger_.debug("Conceptual: Remove src_port_range for rule " + std::to_string(rule_id) + " from source_port_tree_.");
    }
    // Similar for dest_port_tree_

    // Note: Bloom filter elements are typically not removed. If this rule significantly
    // changes the profile, the Bloom filter might need rebuilding or be a counting variant.
    // For this skeleton, no action for Bloom filter on removal.

    return true;
}

// Placeholder for IP parsing utilities - would need robust implementation
// std::string PacketClassifier::ipPrefixToBitString(const std::string& ip_prefix) { /* ... */ return ""; }
// uint32_t PacketClassifier::ipStringToUint32(const std::string& ip_str) { /* ... */ return 0; }
