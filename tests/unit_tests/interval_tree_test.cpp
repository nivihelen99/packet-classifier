#include "gtest/gtest.h"
#include "data_structures/interval_tree.h" // Adjust path as necessary
#include <vector>
#include <algorithm> // For std::sort, std::find_if

// Helper to compare two Interval structs
bool compareIntervals(const Interval& a, const Interval& b) {
    return a.low == b.low && a.high == b.high && a.data_id == b.data_id;
}

// Helper to check if a vector of intervals contains a specific interval
bool containsInterval(const std::vector<Interval>& vec, const Interval& target) {
    return std::find_if(vec.begin(), vec.end(), [&](const Interval& item) {
        return compareIntervals(item, target);
    }) != vec.end();
}

class IntervalTreeTest : public ::testing::Test {
protected:
    IntervalTree tree;

    // Helper to sort interval vectors for easier comparison
    void sortIntervals(std::vector<Interval>& intervals) {
        std::sort(intervals.begin(), intervals.end());
    }
};

TEST_F(IntervalTreeTest, EmptyTree) {
    EXPECT_TRUE(tree.findOverlappingIntervals(10).empty());
    EXPECT_TRUE(tree.findOverlappingIntervals(0, 100).empty());
    // Remove on empty tree (placeholder remove returns true, so can't assert much)
    EXPECT_TRUE(tree.remove(1, 10, 1)); 
}

TEST_F(IntervalTreeTest, InsertSingleInterval) {
    tree.insert(10, 20, 1);
    
    std::vector<Interval> result_point = tree.findOverlappingIntervals(15);
    ASSERT_EQ(result_point.size(), 1);
    EXPECT_TRUE(compareIntervals(result_point[0], Interval(10, 20, 1)));

    std::vector<Interval> result_interval = tree.findOverlappingIntervals(12, 18);
    ASSERT_EQ(result_interval.size(), 1);
    EXPECT_TRUE(compareIntervals(result_interval[0], Interval(10, 20, 1)));

    EXPECT_TRUE(tree.findOverlappingIntervals(5).empty());
    EXPECT_TRUE(tree.findOverlappingIntervals(25).empty());
    EXPECT_TRUE(tree.findOverlappingIntervals(0, 5).empty());
    EXPECT_TRUE(tree.findOverlappingIntervals(25, 30).empty());
}

TEST_F(IntervalTreeTest, InsertMultipleNonOverlappingIntervals) {
    tree.insert(10, 20, 1);
    tree.insert(30, 40, 2);
    tree.insert(50, 60, 3);

    // Test points
    EXPECT_EQ(tree.findOverlappingIntervals(15).size(), 1);
    EXPECT_TRUE(containsInterval(tree.findOverlappingIntervals(15), Interval(10, 20, 1)));
    EXPECT_EQ(tree.findOverlappingIntervals(35).size(), 1);
    EXPECT_TRUE(containsInterval(tree.findOverlappingIntervals(35), Interval(30, 40, 2)));
    EXPECT_EQ(tree.findOverlappingIntervals(55).size(), 1);
    EXPECT_TRUE(containsInterval(tree.findOverlappingIntervals(55), Interval(50, 60, 3)));
    EXPECT_TRUE(tree.findOverlappingIntervals(25).empty()); // Gap

    // Test intervals
    std::vector<Interval> res1 = tree.findOverlappingIntervals(10, 20);
    ASSERT_EQ(res1.size(), 1);
    EXPECT_TRUE(containsInterval(res1, Interval(10, 20, 1)));

    std::vector<Interval> res_spanning = tree.findOverlappingIntervals(5, 65);
    sortIntervals(res_spanning);
    ASSERT_EQ(res_spanning.size(), 3);
    EXPECT_TRUE(compareIntervals(res_spanning[0], Interval(10, 20, 1)));
    EXPECT_TRUE(compareIntervals(res_spanning[1], Interval(30, 40, 2)));
    EXPECT_TRUE(compareIntervals(res_spanning[2], Interval(50, 60, 3)));
}

TEST_F(IntervalTreeTest, InsertOverlappingIntervals) {
    tree.insert(10, 30, 1030); // A
    tree.insert(20, 40, 2040); // B (overlaps A)
    tree.insert(5, 15, 515);   // C (overlaps A)

    // Point queries
    std::vector<Interval> res_at_12 = tree.findOverlappingIntervals(12); // A, C
    sortIntervals(res_at_12);
    ASSERT_EQ(res_at_12.size(), 2);
    EXPECT_TRUE(containsInterval(res_at_12, Interval(5, 15, 515)));
    EXPECT_TRUE(containsInterval(res_at_12, Interval(10, 30, 1030)));
    
    std::vector<Interval> res_at_25 = tree.findOverlappingIntervals(25); // A, B
    sortIntervals(res_at_25);
    ASSERT_EQ(res_at_25.size(), 2);
    EXPECT_TRUE(containsInterval(res_at_25, Interval(10, 30, 1030)));
    EXPECT_TRUE(containsInterval(res_at_25, Interval(20, 40, 2040)));

    std::vector<Interval> res_at_35 = tree.findOverlappingIntervals(35); // B only
    ASSERT_EQ(res_at_35.size(), 1);
    EXPECT_TRUE(containsInterval(res_at_35, Interval(20, 40, 2040)));

    // Interval queries
    std::vector<Interval> res_overlap_all = tree.findOverlappingIntervals(12, 25); // A, B, C
    sortIntervals(res_overlap_all);
    ASSERT_EQ(res_overlap_all.size(), 3);
    EXPECT_TRUE(containsInterval(res_overlap_all, Interval(5, 15, 515)));
    EXPECT_TRUE(containsInterval(res_overlap_all, Interval(10, 30, 1030)));
    EXPECT_TRUE(containsInterval(res_overlap_all, Interval(20, 40, 2040)));
}

TEST_F(IntervalTreeTest, PointIntervals) {
    tree.insert(10, 10, 1); // Point interval
    tree.insert(10, 20, 2); // Regular interval containing the point

    std::vector<Interval> res_at_10 = tree.findOverlappingIntervals(10);
    sortIntervals(res_at_10);
    ASSERT_EQ(res_at_10.size(), 2);
    EXPECT_TRUE(containsInterval(res_at_10, Interval(10, 10, 1)));
    EXPECT_TRUE(containsInterval(res_at_10, Interval(10, 20, 2)));

    std::vector<Interval> res_at_15 = tree.findOverlappingIntervals(15);
    ASSERT_EQ(res_at_15.size(), 1);
    EXPECT_TRUE(containsInterval(res_at_15, Interval(10, 20, 2)));
}

TEST_F(IntervalTreeTest, ContainedIntervals) {
    tree.insert(0, 100, 1);  // Outer
    tree.insert(20, 30, 2); // Inner1
    tree.insert(40, 50, 3); // Inner2

    std::vector<Interval> res_pt_25 = tree.findOverlappingIntervals(25); // Outer, Inner1
    ASSERT_EQ(res_pt_25.size(), 2);
    EXPECT_TRUE(containsInterval(res_pt_25, Interval(0, 100, 1)));
    EXPECT_TRUE(containsInterval(res_pt_25, Interval(20, 30, 2)));
    
    std::vector<Interval> res_interval_25_45 = tree.findOverlappingIntervals(25, 45); // Outer, Inner1, Inner2
    ASSERT_EQ(res_interval_25_45.size(), 3);
    EXPECT_TRUE(containsInterval(res_interval_25_45, Interval(0, 100, 1)));
    EXPECT_TRUE(containsInterval(res_interval_25_45, Interval(20, 30, 2)));
    EXPECT_TRUE(containsInterval(res_interval_25_45, Interval(40, 50, 3)));
}

TEST_F(IntervalTreeTest, AdjacentIntervals) {
    tree.insert(10, 20, 1);
    tree.insert(21, 30, 2); // Adjacent to first, no overlap

    EXPECT_TRUE(containsInterval(tree.findOverlappingIntervals(20), Interval(10, 20, 1)));
    EXPECT_EQ(tree.findOverlappingIntervals(20).size(), 1);
    
    EXPECT_TRUE(containsInterval(tree.findOverlappingIntervals(21), Interval(21, 30, 2)));
    EXPECT_EQ(tree.findOverlappingIntervals(21).size(), 1);

    // Query an interval that spans the boundary but doesn't touch the numbers
    EXPECT_TRUE(tree.findOverlappingIntervals(0, 9).empty());
    EXPECT_TRUE(tree.findOverlappingIntervals(31, 40).empty());
    
    std::vector<Interval> res_spanning_gap = tree.findOverlappingIntervals(15, 25);
    sortIntervals(res_spanning_gap);
    ASSERT_EQ(res_spanning_gap.size(), 2);
    EXPECT_TRUE(containsInterval(res_spanning_gap, Interval(10, 20, 1)));
    EXPECT_TRUE(containsInterval(res_spanning_gap, Interval(21, 30, 2)));
}

TEST_F(IntervalTreeTest, QueryAtBoundaries) {
    tree.insert(10, 20, 1020);
    
    // Point is exactly at the start
    std::vector<Interval> res_at_low = tree.findOverlappingIntervals(10);
    ASSERT_EQ(res_at_low.size(), 1);
    EXPECT_TRUE(compareIntervals(res_at_low[0], Interval(10, 20, 1020)));

    // Point is exactly at the end
    std::vector<Interval> res_at_high = tree.findOverlappingIntervals(20);
    ASSERT_EQ(res_at_high.size(), 1);
    EXPECT_TRUE(compareIntervals(res_at_high[0], Interval(10, 20, 1020)));

    // Interval query starting exactly at start
    std::vector<Interval> res_int_from_low = tree.findOverlappingIntervals(10, 15);
    ASSERT_EQ(res_int_from_low.size(), 1);
    EXPECT_TRUE(compareIntervals(res_int_from_low[0], Interval(10, 20, 1020)));

    // Interval query ending exactly at end
    std::vector<Interval> res_int_to_high = tree.findOverlappingIntervals(15, 20);
    ASSERT_EQ(res_int_to_high.size(), 1);
    EXPECT_TRUE(compareIntervals(res_int_to_high[0], Interval(10, 20, 1020)));
}

TEST_F(IntervalTreeTest, BasicRemove) {
    tree.insert(10, 20, 1);
    tree.insert(30, 40, 2);
    tree.insert(5, 15, 3); // Overlaps with [10,20,1]

    // The remove function in placeholder returns true always.
    // We test by querying after removal.
    tree.remove(30, 40, 2);
    EXPECT_TRUE(tree.findOverlappingIntervals(35).empty()); // Should be gone
    ASSERT_EQ(tree.findOverlappingIntervals(12).size(), 2); // The other two should remain

    tree.remove(10, 20, 1);
    std::vector<Interval> res_at_12 = tree.findOverlappingIntervals(12);
    ASSERT_EQ(res_at_12.size(), 1); // Only [5,15,3] should remain for point 12
    EXPECT_TRUE(compareIntervals(res_at_12[0], Interval(5, 15, 3)));
    
    EXPECT_TRUE(tree.findOverlappingIntervals(17).empty()); // Point 17 was only in [10,20,1]
}

TEST_F(IntervalTreeTest, RemoveNonExistent) {
    tree.insert(10, 20, 1);
    // Try removing an interval that wasn't inserted
    tree.remove(100, 200, 100);
    // Try removing an interval with correct range but different data_id
    tree.remove(10, 20, 2); // data_id is different
    
    std::vector<Interval> results = tree.findOverlappingIntervals(15);
    ASSERT_EQ(results.size(), 1); // Original interval should still be there
    EXPECT_TRUE(compareIntervals(results[0], Interval(10, 20, 1)));
}

TEST_F(IntervalTreeTest, RemoveAndReinsert) {
    tree.insert(10, 20, 1);
    tree.remove(10, 20, 1);
    EXPECT_TRUE(tree.findOverlappingIntervals(15).empty());

    tree.insert(10, 20, 1); // Re-insert same interval
    std::vector<Interval> results = tree.findOverlappingIntervals(15);
    ASSERT_EQ(results.size(), 1);
    EXPECT_TRUE(compareIntervals(results[0], Interval(10, 20, 1)));
}

TEST_F(IntervalTreeTest, InsertInvalidInterval) {
    tree.insert(20, 10, 1); // low > high
    EXPECT_TRUE(tree.findOverlappingIntervals(15).empty()); // Should not have been inserted
    EXPECT_TRUE(tree.findOverlappingIntervals(0,100).empty());
}

TEST_F(IntervalTreeTest, QueryInvalidInterval) {
    tree.insert(10, 20, 1);
    std::vector<Interval> results = tree.findOverlappingIntervals(30, 25); // low > high
    EXPECT_TRUE(results.empty());
}

// Stress test for balancing - insert many intervals
TEST_F(IntervalTreeTest, InsertManyIntervals) {
    const int count = 100;
    for (int i = 0; i < count; ++i) {
        tree.insert(i * 10, i * 10 + 5, i); // e.g., [0,5], [10,15], [20,25] ...
    }

    for (int i = 0; i < count; ++i) {
        std::vector<Interval> result = tree.findOverlappingIntervals(i * 10 + 2); // Point within each interval
        ASSERT_EQ(result.size(), 1) << "Failed for point query on interval " << i;
        EXPECT_TRUE(compareIntervals(result[0], Interval(i * 10, i * 10 + 5, i)));

        std::vector<Interval> result_interval = tree.findOverlappingIntervals(i * 10, i * 10 + 5);
        ASSERT_EQ(result_interval.size(), 1) << "Failed for interval query on interval " << i;
        EXPECT_TRUE(compareIntervals(result_interval[0], Interval(i * 10, i * 10 + 5, i)));
    }

    // Remove all of them
    for (int i = 0; i < count; ++i) {
        tree.remove(i * 10, i * 10 + 5, i);
        EXPECT_TRUE(tree.findOverlappingIntervals(i * 10 + 2).empty()) << "Interval " << i << " not removed.";
    }
    EXPECT_TRUE(tree.findOverlappingIntervals(0, count * 10 + 5).empty()); // Tree should be empty
}

// Test a sequence of inserts and removes that might challenge AVL balancing
TEST_F(IntervalTreeTest, MixedOperationsBalancing) {
    tree.insert(10, 20, 1);
    tree.insert(5, 15, 2);  // Overlaps, should cause rotations if needed
    tree.insert(25, 35, 3);
    
    // Initial checks
    ASSERT_EQ(tree.findOverlappingIntervals(12).size(), 2); // 1, 2
    ASSERT_EQ(tree.findOverlappingIntervals(30).size(), 1); // 3

    tree.remove(10, 20, 1); // Remove one that might be a parent or involved in rotation
    
    std::vector<Interval> r1 = tree.findOverlappingIntervals(12);
    ASSERT_EQ(r1.size(), 1);
    EXPECT_TRUE(containsInterval(r1, Interval(5,15,2)));
    
    std::vector<Interval> r2 = tree.findOverlappingIntervals(30);
    ASSERT_EQ(r2.size(), 1);
    EXPECT_TRUE(containsInterval(r2, Interval(25,35,3)));

    tree.insert(1, 50, 4); // Broad interval
    ASSERT_EQ(tree.findOverlappingIntervals(12).size(), 2); // 2, 4
    ASSERT_EQ(tree.findOverlappingIntervals(30).size(), 2); // 3, 4

    tree.remove(5, 15, 2);
    tree.remove(25, 35, 3);
    ASSERT_EQ(tree.findOverlappingIntervals(12).size(), 1); // 4
    ASSERT_EQ(tree.findOverlappingIntervals(30).size(), 1); // 4
    
    std::vector<Interval> r3 = tree.findOverlappingIntervals(25);
    ASSERT_EQ(r3.size(), 1);
    EXPECT_TRUE(containsInterval(r3, Interval(1,50,4)));

    tree.remove(1, 50, 4);
    EXPECT_TRUE(tree.findOverlappingIntervals(25).empty());
}

// int main(int argc, char **argv) {
//     ::testing::InitGoogleTest(&argc, argv);
//     return RUN_ALL_TESTS();
// }
