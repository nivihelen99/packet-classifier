#include "gtest/gtest.h"
#include "utils/rule_manager.h"
#include "packet_classifier.h" // For ClassificationRule, PacketFilter, ActionList
#include <algorithm> // For std::find_if

// Helper function to create a ClassificationRule with some defaults
ClassificationRule createTestRule(
    int id, int priority,
    const std::string& src_ip = "", const std::string& dst_ip = "",
    uint16_t src_port_low = 0, uint16_t src_port_high = 0,
    uint16_t dst_port_low = 0, uint16_t dst_port_high = 0,
    uint8_t proto = 0,
    ActionList::ActionType action_type = ActionList::ActionType::DROP) {
    
    PacketFilter filter;
    filter.source_ip_prefix = src_ip;
    filter.dest_ip_prefix = dst_ip;
    filter.source_port_low = src_port_low;
    filter.source_port_high = src_port_high;
    filter.dest_port_low = dst_port_low;
    filter.dest_port_high = dst_port_high;
    filter.protocol = proto;

    ActionList actions;
    actions.primary_action = action_type;
    if (action_type == ActionList::ActionType::FORWARD) {
        actions.next_hop_id = 123; // Some default next_hop
    }
    
    return ClassificationRule(id, priority, filter, actions);
}

// Helper to compare two ClassificationRule objects (excluding stats for simplicity in some tests)
bool compareRules(const ClassificationRule& r1, const ClassificationRule& r2, bool include_stats = false) {
    bool match = r1.rule_id == r2.rule_id &&
           r1.priority == r2.priority &&
           r1.enabled == r2.enabled &&
           // Compare PacketFilter
           r1.filter.source_ip_prefix == r2.filter.source_ip_prefix &&
           r1.filter.dest_ip_prefix == r2.filter.dest_ip_prefix &&
           r1.filter.source_port_low == r2.filter.source_port_low &&
           r1.filter.source_port_high == r2.filter.source_port_high &&
           r1.filter.dest_port_low == r2.filter.dest_port_low &&
           r1.filter.dest_port_high == r2.filter.dest_port_high &&
           r1.filter.protocol == r2.filter.protocol &&
           // Compare ActionList
           r1.actions.primary_action == r2.actions.primary_action &&
           r1.actions.next_hop_id == r2.actions.next_hop_id &&
           r1.actions.log_identifier == r2.actions.log_identifier;
    
    if (include_stats) {
        match = match && r1.match_count == r2.match_count;
        // last_match_time is harder to compare exactly, might skip or check for non-zero
        if (r1.last_match_time > 0 && r2.last_match_time > 0) { // if both set, good enough for some tests
             match = match && (r1.last_match_time == r2.last_match_time);
        } else if (r1.last_match_time == 0 && r2.last_match_time == 0) {
            // both zero is fine
        } else {
            match = false; // one is zero, other is not
        }
    }
    return match;
}


class RuleManagerTest : public ::testing::Test {
protected:
    RuleManager rm;
    Logger& logger_ = Logger::getInstance(); // For controlling log levels during tests if needed
    LogLevel original_log_level_;

    void SetUp() override {
        original_log_level_ = logger_.getLogLevel();
        // logger_.setLogLevel(LogLevel::TRACE); // Enable detailed logs from RuleManager for debugging tests
                                             // Or set to NONE to quiet them down unless specifically testing logs
    }

    void TearDown() override {
        logger_.setLogLevel(original_log_level_);
    }
};

TEST_F(RuleManagerTest, InitialState) {
    EXPECT_EQ(rm.getRule(1), nullptr);
    EXPECT_TRUE(rm.getRulesByPriority().empty());
    EXPECT_TRUE(rm.getAllRules().empty());
}

TEST_F(RuleManagerTest, AddRule) {
    ClassificationRule rule1 = createTestRule(1, 100, "192.168.1.0/24");
    EXPECT_TRUE(rm.addRule(rule1));
    
    const ClassificationRule* retrieved_rule = rm.getRule(1);
    ASSERT_NE(retrieved_rule, nullptr);
    EXPECT_TRUE(compareRules(rule1, *retrieved_rule));
}

TEST_F(RuleManagerTest, AddDuplicateRuleID) {
    ClassificationRule rule1 = createTestRule(1, 100);
    EXPECT_TRUE(rm.addRule(rule1));
    ClassificationRule rule2_dup_id = createTestRule(1, 200); // Same ID, different priority
    EXPECT_FALSE(rm.addRule(rule2_dup_id)); // Should fail
    
    const ClassificationRule* retrieved_rule = rm.getRule(1);
    ASSERT_NE(retrieved_rule, nullptr);
    EXPECT_EQ(retrieved_rule->priority, 100); // Original rule should remain
}

TEST_F(RuleManagerTest, GetNonExistentRule) {
    EXPECT_EQ(rm.getRule(999), nullptr);
}

TEST_F(RuleManagerTest, DeleteRule) {
    ClassificationRule rule1 = createTestRule(1, 100);
    rm.addRule(rule1);
    ASSERT_NE(rm.getRule(1), nullptr);

    EXPECT_TRUE(rm.deleteRule(1));
    EXPECT_EQ(rm.getRule(1), nullptr);
    EXPECT_TRUE(rm.getRulesByPriority().empty());
}

TEST_F(RuleManagerTest, DeleteNonExistentRule) {
    EXPECT_FALSE(rm.deleteRule(999));
}

TEST_F(RuleManagerTest, ModifyRule) {
    ClassificationRule rule1 = createTestRule(1, 100, "1.1.1.1/32", "2.2.2.2/32");
    rm.addRule(rule1);

    ClassificationRule modified_rule_data = createTestRule(1, 200, "3.3.3.3/32", "4.4.4.4/32", 0,0,0,0,6, ActionList::ActionType::FORWARD);
    // RuleManager::modifyRule uses the rule_id parameter, not the one in new_rule_data.
    // So, createTestRule's ID for modified_rule_data doesn't strictly matter here if we pass '1' to modifyRule.
    
    EXPECT_TRUE(rm.modifyRule(1, modified_rule_data));
    
    const ClassificationRule* retrieved_rule = rm.getRule(1);
    ASSERT_NE(retrieved_rule, nullptr);
    EXPECT_EQ(retrieved_rule->priority, 200);
    EXPECT_EQ(retrieved_rule->filter.source_ip_prefix, "3.3.3.3/32");
    EXPECT_EQ(retrieved_rule->actions.primary_action, ActionList::ActionType::FORWARD);
    EXPECT_EQ(retrieved_rule->rule_id, 1); // ID must remain 1
}

TEST_F(RuleManagerTest, ModifyRuleWithSameIDInData) {
    ClassificationRule rule1 = createTestRule(10, 100);
    rm.addRule(rule1);
    ClassificationRule modified_rule_data = createTestRule(10, 150); // rule_id in data is 10
    
    EXPECT_TRUE(rm.modifyRule(10, modified_rule_data)); // Modifying rule 10
    const ClassificationRule* retrieved = rm.getRule(10);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->priority, 150);
}


TEST_F(RuleManagerTest, ModifyNonExistentRule) {
    ClassificationRule rule_data = createTestRule(1, 100);
    EXPECT_FALSE(rm.modifyRule(999, rule_data));
}

TEST_F(RuleManagerTest, GetAllRules) {
    EXPECT_TRUE(rm.getAllRules().empty());
    ClassificationRule rule1 = createTestRule(1, 100);
    ClassificationRule rule2 = createTestRule(2, 200);
    rm.addRule(rule1);
    rm.addRule(rule2);

    std::map<int, const ClassificationRule*> all_rules = rm.getAllRules();
    ASSERT_EQ(all_rules.size(), 2);
    ASSERT_NE(all_rules.find(1), all_rules.end());
    ASSERT_NE(all_rules.find(2), all_rules.end());
    EXPECT_TRUE(compareRules(rule1, *all_rules.at(1)));
    EXPECT_TRUE(compareRules(rule2, *all_rules.at(2)));
}

TEST_F(RuleManagerTest, GetRulesByPriority) {
    ClassificationRule r1 = createTestRule(1, 100);
    ClassificationRule r2 = createTestRule(2, 300);
    ClassificationRule r3 = createTestRule(3, 200);
    rm.addRule(r1);
    rm.addRule(r2);
    rm.addRule(r3);

    std::vector<const ClassificationRule*> sorted_rules = rm.getRulesByPriority();
    ASSERT_EQ(sorted_rules.size(), 3);
    EXPECT_EQ(sorted_rules[0]->rule_id, 2); // Priority 300
    EXPECT_EQ(sorted_rules[1]->rule_id, 3); // Priority 200
    EXPECT_EQ(sorted_rules[2]->rule_id, 1); // Priority 100

    // Modify priority of r1 to be the highest
    ClassificationRule modified_r1 = createTestRule(1, 400);
    rm.modifyRule(1, modified_r1);
    
    sorted_rules = rm.getRulesByPriority();
    ASSERT_EQ(sorted_rules.size(), 3);
    EXPECT_EQ(sorted_rules[0]->rule_id, 1); // Priority 400
    EXPECT_EQ(sorted_rules[1]->rule_id, 2); // Priority 300
    EXPECT_EQ(sorted_rules[2]->rule_id, 3); // Priority 200
}

TEST_F(RuleManagerTest, StatisticsManagement) {
    ClassificationRule rule1 = createTestRule(1, 100);
    rm.addRule(rule1);

    const uint64_t time1 = 1234567890;
    const uint64_t time2 = 1234567990;

    EXPECT_TRUE(rm.incrementRuleMatchCount(1, time1));
    const ClassificationRule* r = rm.getRule(1);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->match_count, 1);
    EXPECT_EQ(r->last_match_time, time1);

    EXPECT_TRUE(rm.incrementRuleMatchCount(1, time2));
    r = rm.getRule(1); // Re-fetch
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->match_count, 2);
    EXPECT_EQ(r->last_match_time, time2);

    EXPECT_TRUE(rm.resetRuleStatistics(1));
    r = rm.getRule(1); // Re-fetch
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->match_count, 0);
    EXPECT_EQ(r->last_match_time, 0);

    // Test on non-existent rule
    EXPECT_FALSE(rm.incrementRuleMatchCount(99, time1));
    EXPECT_FALSE(rm.resetRuleStatistics(99));
}

TEST_F(RuleManagerTest, ResetAllStatistics) {
    ClassificationRule r1 = createTestRule(1, 100);
    ClassificationRule r2 = createTestRule(2, 200);
    rm.addRule(r1);
    rm.addRule(r2);

    rm.incrementRuleMatchCount(1, 1000);
    rm.incrementRuleMatchCount(2, 2000);

    EXPECT_TRUE(rm.resetAllRuleStatistics());

    const ClassificationRule* ret_r1 = rm.getRule(1);
    const ClassificationRule* ret_r2 = rm.getRule(2);
    ASSERT_NE(ret_r1, nullptr);
    ASSERT_NE(ret_r2, nullptr);
    EXPECT_EQ(ret_r1->match_count, 0);
    EXPECT_EQ(ret_r1->last_match_time, 0);
    EXPECT_EQ(ret_r2->match_count, 0);
    EXPECT_EQ(ret_r2->last_match_time, 0);
}

TEST_F(RuleManagerTest, ConflictDetectionPlaceholder) {
    // Current detectConflict always returns false.
    ClassificationRule rule1 = createTestRule(1, 100, "1.1.1.0/24");
    EXPECT_FALSE(rm.detectConflict(rule1)); // Public method
    
    // Test that addRule calls it (and succeeds because it's false)
    EXPECT_TRUE(rm.addRule(rule1)); 
    
    ClassificationRule rule2_potential_conflict = createTestRule(2, 100, "1.1.1.0/24"); // Same filter
    EXPECT_FALSE(rm.detectConflict(rule2_potential_conflict));
    EXPECT_TRUE(rm.addRule(rule2_potential_conflict)); // Will be added as IDs are different
}

// int main(int argc, char **argv) {
//     ::testing::InitGoogleTest(&argc, argv);
//     return RUN_ALL_TESTS();
// }
