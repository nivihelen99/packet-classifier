#ifndef RULE_MANAGER_H
#define RULE_MANAGER_H

#include "utils/threading.h"   // For ReadWriteLock
#include "utils/logging.h"     // For Logger
#include <vector>
#include <cstdint> // For uint64_t
// Forward declare ClassificationRule to avoid circular dependency with packet_classifier.h
// packet_classifier.h will include rule_manager.h
#include <map>
#include <memory> // For std::shared_ptr if rules are managed that way
#include <algorithm> // For std::sort, std::find_if

struct ClassificationRule; // Forward declaration from packet_classifier.h

class RuleManager {
public:
    RuleManager();
    ~RuleManager();

    // Rule Management API
    bool addRule(const ClassificationRule& rule);
    bool deleteRule(int rule_id); // Changed from uint32_t to int to match ClassificationRule::rule_id
    bool modifyRule(int rule_id, const ClassificationRule& new_rule_data);

    // Rule Access API
    // Returns a const pointer to avoid modification outside RuleManager's controlled methods.
    // Returns nullptr if rule_id not found.
    const ClassificationRule* getRule(int rule_id) const; 
    
    // Returns a list of const pointers to rules, sorted by priority.
    // The list itself is a copy of pointers, providing a snapshot.
    std::vector<const ClassificationRule*> getRulesByPriority() const;

    // Statistics Management (to be called by PacketClassifier or other relevant modules)
    bool incrementRuleMatchCount(int rule_id, uint64_t timestamp);
    bool resetRuleStatistics(int rule_id); // Resets stats for a single rule
    bool resetAllRuleStatistics();      // Resets stats for all rules

    // Conflict Detection (Placeholder)
    // Returns true if the given rule conflicts with existing rules.
    bool detectConflict(const ClassificationRule& rule) const;

    // Get all rules (e.g. for statistics or full export)
    // Returns a map of rule_id to const ClassificationRule pointers
    std::map<int, const ClassificationRule*> getAllRules() const;


private:
    // Primary storage for rules. Using value semantics for rules directly in the map.
    std::map<int, ClassificationRule> rules_by_id_;
    
    // For quick, sorted access by priority. This vector will store pointers
    // to the ClassificationRule objects held in rules_by_id_.
    std::vector<ClassificationRule*> rules_by_priority_cache_;
    
    // Rebuilds rules_by_priority_cache_ from rules_by_id_
    void rebuildPriorityCache();

    mutable ReadWriteLock rw_lock_; // Protects rules_by_id_ and rules_by_priority_cache_
    Logger& logger_;
};

#endif // RULE_MANAGER_H
