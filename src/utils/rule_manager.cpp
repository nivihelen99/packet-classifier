#include "utils/rule_manager.h"
#include "packet_classifier.h" // For full definition of ClassificationRule
#include <iostream> // For logging placeholders if logger isn't fully used yet

RuleManager::RuleManager() : logger_(Logger::getInstance()) {
    logger_.info("RuleManager initialized.");
}

RuleManager::~RuleManager() {
    logger_.info("RuleManager destroyed.");
}

bool RuleManager::addRule(const ClassificationRule& rule) {
    WriteLockGuard lock(rw_lock_);
    logger_.debug("RuleManager: Attempting to add rule ID: " + std::to_string(rule.rule_id));

    if (rules_by_id_.count(rule.rule_id)) {
        logger_.warning("RuleManager: Failed to add rule. Rule ID " + std::to_string(rule.rule_id) + " already exists.");
        return false;
    }

    if (detectConflict(rule)) { // Placeholder call
        logger_.warning("RuleManager: Failed to add rule ID " + std::to_string(rule.rule_id) + ". Conflict detected.");
        return false;
    }

    auto pair = rules_by_id_.emplace(rule.rule_id, rule);
    if(pair.second){ // Check if emplace was successful
        rebuildPriorityCache();
        logger_.info("RuleManager: Added rule ID: " + std::to_string(rule.rule_id) + " successfully.");
        return true;
    } else {
        logger_.error("RuleManager: Failed to emplace rule ID " + std::to_string(rule.rule_id) + " into map for unknown reasons.");
        return false;
    }
}

bool RuleManager::deleteRule(int rule_id) {
    WriteLockGuard lock(rw_lock_);
    logger_.debug("RuleManager: Attempting to delete rule ID: " + std::to_string(rule_id));

    if (rules_by_id_.erase(rule_id) > 0) { // erase returns number of elements removed
        rebuildPriorityCache();
        logger_.info("RuleManager: Deleted rule ID: " + std::to_string(rule_id) + " successfully.");
        return true;
    } else {
        logger_.warning("RuleManager: Failed to delete rule. Rule ID " + std::to_string(rule_id) + " not found.");
        return false;
    }
}

bool RuleManager::modifyRule(int rule_id, const ClassificationRule& new_rule_data) {
    WriteLockGuard lock(rw_lock_);
    logger_.debug("RuleManager: Attempting to modify rule ID: " + std::to_string(rule_id));

    auto it = rules_by_id_.find(rule_id);
    if (it == rules_by_id_.end()) {
        logger_.warning("RuleManager: Failed to modify rule. Rule ID " + std::to_string(rule_id) + " not found.");
        return false;
    }

    // Create a temporary rule to check for conflicts, excluding the rule being modified.
    // This is a simplified conflict check logic.
    ClassificationRule temp_new_rule = new_rule_data;
    temp_new_rule.rule_id = rule_id; // Ensure ID is correct for conflict check context

    // Temporarily remove the old rule to avoid self-conflict detection if detectConflict is sophisticated
    // ClassificationRule old_rule_copy = it->second; // Save for restoration if needed
    // rules_by_id_.erase(it); // Temporarily remove
    // bool conflict = detectConflict(temp_new_rule);
    // rules_by_id_.emplace(rule_id, old_rule_copy); // Restore (or emplace new if no conflict)
    // For simpler placeholder detectConflict, this might not be needed.

    // Pass the original rule_id to detectConflict so it can ignore the rule being modified if necessary.
    if (detectConflict(temp_new_rule)) { 
         // Ensure the rule being modified is not considered in conflict with itself if `detectConflict` isn't smart about it.
         // This depends on detectConflict implementation. For now, assume it's a simple check.
        logger_.warning("RuleManager: Failed to modify rule ID " + std::to_string(rule_id) + ". Conflict detected with new data.");
        return false;
    }
    
    // Check if priority changed, which would necessitate rebuilding the cache.
    bool priority_changed = (it->second.priority != new_rule_data.priority);

    // Update rule data
    it->second = new_rule_data;
    it->second.rule_id = rule_id; // Ensure original rule_id is maintained

    if (priority_changed || rules_by_priority_cache_.empty()) { // Rebuild if priority changed or cache is not yet built
        rebuildPriorityCache();
    }
    // If only content changed but not priority, and cache stores pointers, it's still valid.
    // However, if rules_by_priority_cache_ stores copies, it needs rebuild.
    // Since it stores pointers, we only *need* to rebuild if order changes (priority)
    // or if elements are added/removed. For modify, only priority matters for the cache.
    // For simplicity and safety, one might rebuild anyway if any part of rule changes that could affect implicit ordering
    // or if other structures depend on rule content. Here, explicit check on priority.

    logger_.info("RuleManager: Modified rule ID: " + std::to_string(rule_id) + " successfully.");
    return true;
}

const ClassificationRule* RuleManager::getRule(int rule_id) const {
    ReadLockGuard lock(rw_lock_);
    auto it = rules_by_id_.find(rule_id);
    if (it != rules_by_id_.end()) {
        return &(it->second);
    }
    logger_.debug("RuleManager: getRule for ID " + std::to_string(rule_id) + " not found.");
    return nullptr;
}

std::vector<const ClassificationRule*> RuleManager::getRulesByPriority() const {
    ReadLockGuard lock(rw_lock_);
    // Return a copy of the pointers, so the caller has a stable list
    // even if the lock is released and another thread modifies rules_by_priority_cache_.
    // The ClassificationRule objects themselves are protected by the RuleManager's write lock
    // for modification, so their content (seen via these const pointers) is stable during the read lock.
    std::vector<const ClassificationRule*> rules_snapshot;
    rules_snapshot.reserve(rules_by_priority_cache_.size());
    for(const auto* rule_ptr : rules_by_priority_cache_){
        rules_snapshot.push_back(rule_ptr);
    }
    return rules_snapshot;
    // Alternative: return rules_by_priority_cache_ directly if caller is trusted or lifetime is managed.
    // For safety, a copy is better.
}

std::map<int, const ClassificationRule*> RuleManager::getAllRules() const {
    ReadLockGuard lock(rw_lock_);
    std::map<int, const ClassificationRule*> all_rules_snapshot;
    for(const auto& pair : rules_by_id_){
        all_rules_snapshot.emplace(pair.first, &pair.second);
    }
    return all_rules_snapshot;
}


bool RuleManager::detectConflict(const ClassificationRule& /*rule*/) const {
    // Placeholder for conflict detection logic.
    // This could involve checking for overlapping filters with same priority, etc.
    // For now, assume no conflicts.
    // A real implementation would iterate through existing rules and compare filter criteria.
    ReadLockGuard lock(rw_lock_); // ensure thread safety if accessing shared rule data
    logger_.debug("RuleManager: detectConflict called (placeholder - always returns false).");
    // A real implementation would iterate through rules_by_id_ or rules_by_priority_cache_
    // and compare filter criteria of 'rule' with existing rules.
    return false; 
}

// --- Statistics Management ---
bool RuleManager::incrementRuleMatchCount(int rule_id, uint64_t timestamp) {
    WriteLockGuard lock(rw_lock_); // Need write lock to modify rule's statistics
    auto it = rules_by_id_.find(rule_id);
    if (it != rules_by_id_.end()) {
        it->second.match_count++;
        it->second.last_match_time = timestamp;
        // logger_.trace("RuleManager: Incremented match count for rule ID: " + std::to_string(rule_id));
        return true;
    }
    logger_.warning("RuleManager: Failed to increment match count. Rule ID " + std::to_string(rule_id) + " not found.");
    return false;
}

bool RuleManager::resetRuleStatistics(int rule_id) {
    WriteLockGuard lock(rw_lock_);
    auto it = rules_by_id_.find(rule_id);
    if (it != rules_by_id_.end()) {
        it->second.match_count = 0;
        it->second.last_match_time = 0;
        logger_.info("RuleManager: Reset statistics for rule ID: " + std::to_string(rule_id));
        return true;
    }
    logger_.warning("RuleManager: Failed to reset statistics. Rule ID " + std::to_string(rule_id) + " not found.");
    return false;
}

bool RuleManager::resetAllRuleStatistics() {
    WriteLockGuard lock(rw_lock_);
    logger_.info("RuleManager: Resetting statistics for all rules.");
    for (auto& pair : rules_by_id_) {
        pair.second.match_count = 0;
        pair.second.last_match_time = 0;
    }
    return true;
}

// --- Private Methods ---
void RuleManager::rebuildPriorityCache() {
    // This method assumes the caller (addRule, deleteRule, modifyRule) holds the WriteLock.
    logger_.trace("RuleManager: Rebuilding priority cache.");
    rules_by_priority_cache_.clear();
    rules_by_priority_cache_.reserve(rules_by_id_.size());
    for (auto& pair : rules_by_id_) {
        rules_by_priority_cache_.push_back(&pair.second);
    }
    // Sort by priority (higher value = higher priority)
    std::sort(rules_by_priority_cache_.begin(), rules_by_priority_cache_.end(),
              [](const ClassificationRule* a, const ClassificationRule* b) {
                  return a->priority > b->priority;
              });
}
