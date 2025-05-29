#ifndef INTERVAL_TREE_H
#define INTERVAL_TREE_H

#include <vector>
#include <algorithm> // For std::sort, std::max
#include <memory>    // For std::unique_ptr

// Define the structure for an interval
struct Interval {
    int low;  // Start of the interval (e.g., start port, start IP address component)
    int high; // End of the interval (e.g., end port, end IP address component)
    // Potentially, store associated data with the interval
    // For example, a policy ID, a VLAN ID, or a pointer to more complex data.
    int data_id; // Example: an identifier for what this interval represents

    Interval(int l, int h, int d = 0) : low(l), high(h), data_id(d) {}

    // For comparing intervals, e.g., for sorting or within the tree structure
    bool operator<(const Interval& other) const {
        if (low != other.low) {
            return low < other.low;
        }
        return high < other.high;
    }
};

// Node structure for the Interval Tree
struct IntervalNode {
    std::unique_ptr<Interval> interval; // Stores a single interval, typically the median point
    int max_high;                       // Maximum high value in the subtree rooted at this node
    std::unique_ptr<IntervalNode> left;
    std::unique_ptr<IntervalNode> right;

    // For balanced tree properties (e.g., AVL, Red-Black)
    int height; // For AVL tree balancing factor or Red-Black color

    // If intervals are stored at nodes based on their 'low' value:
    // std::vector<Interval> intervals_at_node; // Stores intervals that overlap with the node's point

    IntervalNode(std::unique_ptr<Interval> i) : interval(std::move(i)), max_high(0), height(1) {
        if (interval) {
            max_high = interval->high;
        }
    }
    // Update max_high based on children
    void updateMaxHigh() {
        max_high = interval ? interval->high : -1; // Initialize with own interval's high
        if (left) {
            max_high = std::max(max_high, left->max_high);
        }
        if (right) {
            max_high = std::max(max_high, right->max_high);
        }
    }
};


class IntervalTree {
public:
    IntervalTree();
    ~IntervalTree();

    // --- Core Functionality ---
    // Insert an interval into the tree
    void insert(int low, int high, int data_id = 0);
    void insert(const Interval& new_interval);

    // Remove an interval from the tree
    // Removal can be complex, might involve finding the exact interval or all matching ones.
    bool remove(int low, int high, int data_id = 0);
    bool remove(const Interval& target_interval);

    // Query for intervals that overlap with a given point
    std::vector<Interval> findOverlappingIntervals(int point) const;

    // Query for intervals that overlap with a given interval
    std::vector<Interval> findOverlappingIntervals(int low, int high) const;
    std::vector<Interval> findOverlappingIntervals(const Interval& query_interval) const;

    // --- Balanced Tree Maintenance (placeholders/internal) ---
    // These would be called internally by insert/remove.
    // The specific balancing mechanism (AVL, Red-Black) will determine these methods.
private:
    std::unique_ptr<IntervalNode> root;

    // Recursive helper functions
    std::unique_ptr<IntervalNode> insertRecursive(std::unique_ptr<IntervalNode> node, std::unique_ptr<Interval> new_interval);
    std::unique_ptr<IntervalNode> removeRecursive(std::unique_ptr<IntervalNode> node, const Interval& target_interval);
    void findOverlappingRecursive(const IntervalNode* node, int point, std::vector<Interval>& result) const;
    void findOverlappingRecursive(const IntervalNode* node, const Interval& query_interval, std::vector<Interval>& result) const;

    // Balancing specific methods (e.g., for AVL)
    int getHeight(const IntervalNode* node) const;
    int getBalanceFactor(const IntervalNode* node) const;
    void updateNodeHeight(IntervalNode* node); // Renamed from updateHeight for clarity

    std::unique_ptr<IntervalNode> rotateRight(std::unique_ptr<IntervalNode> y);
    std::unique_ptr<IntervalNode> rotateLeft(std::unique_ptr<IntervalNode> x);
    
    // Helper to find the node with the minimum value in a subtree (for deletion)
    IntervalNode* findMin(IntervalNode* node) const;
};

#endif // INTERVAL_TREE_H
