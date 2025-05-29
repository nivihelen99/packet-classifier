#include "data_structures/interval_tree.h"
#include <iostream> // For placeholder output
#include <stdexcept> // For potential errors

// --- IntervalTree Constructor & Destructor ---
IntervalTree::IntervalTree() : root(nullptr) {
    std::cout << "IntervalTree initialized." << std::endl;
}

IntervalTree::~IntervalTree() {
    std::cout << "IntervalTree destroyed." << std::endl;
    // std::unique_ptr will handle recursive deletion of nodes.
}

// --- Core Functionality: Public Methods ---
void IntervalTree::insert(int low, int high, int data_id) {
    if (low > high) {
        std::cerr << "Warning: Interval low (" << low << ") is greater than high (" << high << "). Not inserting." << std::endl;
        return;
    }
    auto new_interval = std::make_unique<Interval>(low, high, data_id);
    std::cout << "Inserting interval: [" << low << ", " << high << "] with data_id: " << data_id << std::endl;
    root = insertRecursive(std::move(root), std::move(new_interval));
}

void IntervalTree::insert(const Interval& new_interval_obj) {
    if (new_interval_obj.low > new_interval_obj.high) {
        std::cerr << "Warning: Interval low is greater than high. Not inserting." << std::endl;
        return;
    }
    auto new_interval_ptr = std::make_unique<Interval>(new_interval_obj.low, new_interval_obj.high, new_interval_obj.data_id);
    std::cout << "Inserting interval object: [" << new_interval_obj.low << ", " << new_interval_obj.high << "] with data_id: " << new_interval_obj.data_id << std::endl;
    root = insertRecursive(std::move(root), std::move(new_interval_ptr));
}

bool IntervalTree::remove(int low, int high, int data_id) {
    Interval target_interval(low, high, data_id);
    std::cout << "Attempting to remove interval: [" << low << ", " << high << "] with data_id: " << data_id << std::endl;
    // To properly check if removal happened, we might need more info from removeRecursive
    // For now, assume if root changes or target is found, it's a "success" in some sense.
    size_t initial_count = 0; // Placeholder: need a way to count nodes or check presence
    // auto old_root_ptr = root.get(); // Not robust
    root = removeRecursive(std::move(root), target_interval);
    // bool removed = (old_root_ptr != root.get() || check_if_really_removed); // Needs better check
    // For skeleton, let's assume it could be removed.
    return true; // Placeholder
}

bool IntervalTree::remove(const Interval& target_interval) {
    std::cout << "Attempting to remove interval object: [" << target_interval.low << ", " << target_interval.high << "]" << std::endl;
    root = removeRecursive(std::move(root), target_interval);
    return true; // Placeholder
}


std::vector<Interval> IntervalTree::findOverlappingIntervals(int point) const {
    std::vector<Interval> result;
    std::cout << "Finding intervals overlapping with point: " << point << std::endl;
    findOverlappingRecursive(root.get(), point, result);
    return result;
}

std::vector<Interval> IntervalTree::findOverlappingIntervals(int low, int high) const {
    std::vector<Interval> result;
    if (low > high) {
         std::cerr << "Warning: Query interval low (" << low << ") is greater than high (" << high << "). Returning empty." << std::endl;
        return result;
    }
    Interval query_interval(low, high);
    std::cout << "Finding intervals overlapping with interval: [" << low << ", " << high << "]" << std::endl;
    findOverlappingRecursive(root.get(), query_interval, result);
    return result;
}

std::vector<Interval> IntervalTree::findOverlappingIntervals(const Interval& query_interval) const {
    std::vector<Interval> result;
     if (query_interval.low > query_interval.high) {
        std::cerr << "Warning: Query interval low is greater than high. Returning empty." << std::endl;
        return result;
    }
    std::cout << "Finding intervals overlapping with interval object: [" << query_interval.low << ", " << query_interval.high << "]" << std::endl;
    findOverlappingRecursive(root.get(), query_interval, result);
    return result;
}

// --- Recursive Helper Functions ---
std::unique_ptr<IntervalNode> IntervalTree::insertRecursive(std::unique_ptr<IntervalNode> node, std::unique_ptr<Interval> new_interval) {
    if (!node) {
        return std::make_unique<IntervalNode>(std::move(new_interval));
    }

    // Standard BST insertion based on the 'low' value of the interval.
    // Other strategies exist (e.g. centering around a point).
    // This implementation sorts nodes primarily by 'low' value.
    if (new_interval->low < node->interval->low) {
        node->left = insertRecursive(std::move(node->left), std::move(new_interval));
    } else if (new_interval->low > node->interval->low) {
        node->right = insertRecursive(std::move(node->right), std::move(new_interval));
    } else { // new_interval->low == node->interval->low
        // Handle duplicate 'low' values. Could compare 'high' or store in a list at the node.
        // For simplicity, let's treat it like greater for now, or update if interval is "same"
        // This part might need refinement based on exact requirements for duplicate intervals.
        if (new_interval->high < node->interval->high) { // Arbitrary tie-breaking
             node->left = insertRecursive(std::move(node->left), std::move(new_interval));
        } else {
             node->right = insertRecursive(std::move(node->right), std::move(new_interval));
        }
        // Or, if exact duplicates are not allowed or should update:
        // if (*new_interval == *node->interval) { node->interval = std::move(new_interval); return node; }
    }

    updateNodeHeight(node.get());
    node->updateMaxHigh(); // Crucial for interval tree queries

    // --- Balancing Logic (AVL style) ---
    int balance = getBalanceFactor(node.get());

    // Left Left Case
    if (balance > 1 && new_interval->low < node->left->interval->low) {
        return rotateRight(std::move(node));
    }
    // Right Right Case
    if (balance < -1 && new_interval->low > node->right->interval->low) {
        return rotateLeft(std::move(node));
    }
    // Left Right Case
    if (balance > 1 && new_interval->low > node->left->interval->low) {
        node->left = rotateLeft(std::move(node->left));
        return rotateRight(std::move(node));
    }
    // Right Left Case
    if (balance < -1 && new_interval->low < node->right->interval->low) {
        node->right = rotateRight(std::move(node->right));
        return rotateLeft(std::move(node));
    }

    return node;
}

IntervalNode* IntervalTree::findMin(IntervalNode* node) const {
    IntervalNode* current = node;
    while (current && current->left) {
        current = current->left.get();
    }
    return current;
}

std::unique_ptr<IntervalNode> IntervalTree::removeRecursive(std::unique_ptr<IntervalNode> node, const Interval& target_interval) {
    if (!node) {
        std::cout << "Target interval for removal not found in this path." << std::endl;
        return nullptr;
    }

    // Find the node to remove
    bool target_is_less = target_interval.low < node->interval->low ||
                          (target_interval.low == node->interval->low && target_interval.high < node->interval->high);
    bool target_is_greater = target_interval.low > node->interval->low ||
                             (target_interval.low == node->interval->low && target_interval.high > node->interval->high);

    if (target_is_less) {
        node->left = removeRecursive(std::move(node->left), target_interval);
    } else if (target_is_greater) {
        node->right = removeRecursive(std::move(node->right), target_interval);
    } else { // Found the node to remove (target_interval matches node->interval)
        if (node->interval->low == target_interval.low && node->interval->high == target_interval.high && node->interval->data_id == target_interval.data_id) {
            // Node with one child or no child
            if (!node->left || !node->right) {
                std::unique_ptr<IntervalNode> temp = std::move(node->left ? node->left : node->right);
                // No child case
                if (!temp) {
                    // node is deleted by unique_ptr going out of scope after return
                    return nullptr; 
                } else { // One child case
                    // node is deleted, temp (child) takes its place
                    return temp; 
                }
            } else { // Node with two children
                IntervalNode* temp_min_node = findMin(node->right.get());
                // Copy the inorder successor's content to this node
                // Important: Create a NEW interval for the current node, don't just copy pointer
                node->interval = std::make_unique<Interval>(*temp_min_node->interval);
                // Delete the inorder successor
                node->right = removeRecursive(std::move(node->right), *temp_min_node->interval);
            }
        } else {
             // Interval values match, but data_id might be different, or it's not the exact one we are looking for.
             // This simplistic BST model for interval tree might require refinement if multiple identical ranges
             // but different data_ids are stored and need precise removal.
             // For now, if interval values match, we assume it's the one (or one of them).
             // If data_id must match, the `else` above is sufficient. If any with matching range is fine, this logic is okay.
             // To be robust, one might search both left/right if only low matches, or store intervals in a list at node.
            std::cout << "Interval range matches, but it might not be the exact target data_id or an exact match is required." << std::endl;
            // Try searching further if the tree allows multiple intervals with same range
            // node->left = removeRecursive(std::move(node->left), target_interval);
            // node->right = removeRecursive(std::move(node->right), target_interval);
            // This part is tricky without a clearer definition of "equality" vs "overlap" for removal.
            // For now, assume the first found matching range is targeted.
        }
    }
    
    // If the tree had only one node then return
    if (!node) return nullptr;


    updateNodeHeight(node.get());
    node->updateMaxHigh();

    // --- Balancing Logic (AVL style) ---
    int balance = getBalanceFactor(node.get());

    // Left Left Case
    if (balance > 1 && getBalanceFactor(node->left.get()) >= 0) {
        return rotateRight(std::move(node));
    }
    // Left Right Case
    if (balance > 1 && getBalanceFactor(node->left.get()) < 0) {
        node->left = rotateLeft(std::move(node->left));
        return rotateRight(std::move(node));
    }
    // Right Right Case
    if (balance < -1 && getBalanceFactor(node->right.get()) <= 0) {
        return rotateLeft(std::move(node));
    }
    // Right Left Case
    if (balance < -1 && getBalanceFactor(node->right.get()) > 0) {
        node->right = rotateRight(std::move(node->right));
        return rotateLeft(std::move(node));
    }

    return node;
}

void IntervalTree::findOverlappingRecursive(const IntervalNode* node, int point, std::vector<Interval>& result) const {
    if (!node) {
        return;
    }

    // If interval at node overlaps with point
    if (node->interval && point >= node->interval->low && point <= node->interval->high) {
        result.push_back(*node->interval);
    }

    // If left child's max_high is greater than point, it might contain an overlapping interval
    if (node->left && node->left->max_high >= point) {
        findOverlappingRecursive(node->left.get(), point, result);
    }

    // If point is to the right of current interval's low, search right.
    // More accurately for interval trees, if point is >= node's interval low, and
    // considering the way max_high is structured, we might need to always check right if
    // point could fall into an interval there.
    // Standard interval tree logic: if point >= node.interval.low, search right.
    // However, with max_high optimization, if point > node.max_high, no need to search.
    // This specific check needs to be adapted for the particular interval tree variant.
    // A common approach:
    // If left child exists and point <= node->left->max_high (or node->interval->low if tree is point-centered)
    //    findOverlappingRecursive(node->left.get(), point, result);
    // If point falls in node->interval, add it.
    // If right child exists and point >= node->interval->low (and possibly other conditions)
    //    findOverlappingRecursive(node->right.get(), point, result);

    // Simplified logic for this skeleton:
    // If point is less than the current interval's low value, and left child exists,
    // only need to check left if point could be in range of left subtree's max_high.
    if (node->left && point < node->interval->low && node->left->max_high >= point) {
         // Already handled by the max_high check above, but being explicit for BST part
    }

    // If point is greater than or equal to current interval's low, search right.
    // (or if point could be in an interval starting later but still overlapping due to its length)
    if (node->right && point >= node->interval->low ) { // Simplified: if point is to the right of start, right subtree could matter
         findOverlappingRecursive(node->right.get(), point, result);
    } else if (node->right && node->right->max_high >=point && point > node->interval->high) {
        // If point is beyond current interval but still within max range of right subtree
        findOverlappingRecursive(node->right.get(), point, result);
    }


}

void IntervalTree::findOverlappingRecursive(const IntervalNode* node, const Interval& query_interval, std::vector<Interval>& result) const {
    if (!node) {
        return;
    }

    // Check if current node's interval overlaps with query_interval
    // Overlap exists if: query_interval.low <= node.interval.high AND query_interval.high >= node.interval.low
    if (node->interval && query_interval.low <= node->interval->high && query_interval.high >= node->interval->low) {
        result.push_back(*node->interval);
    }

    // If left subtree could have an overlap:
    // The query interval's low must be less than or equal to the max high in the left subtree.
    if (node->left && node->left->max_high >= query_interval.low) {
        findOverlappingRecursive(node->left.get(), query_interval, result);
    }

    // If right subtree could have an overlap:
    // The query interval's high must be greater than or equal to the current node's interval low.
    // (This means an interval starting in the right subtree could still overlap the query_interval's start)
    if (node->right && query_interval.high >= node->interval->low) {
         findOverlappingRecursive(node->right.get(), query_interval, result);
    }
}


// --- Balancing Specific Methods (AVL style) ---
int IntervalTree::getHeight(const IntervalNode* node) const {
    return node ? node->height : 0;
}

void IntervalTree::updateNodeHeight(IntervalNode* node) {
    if (node) {
        node->height = 1 + std::max(getHeight(node->left.get()), getHeight(node->right.get()));
    }
}

int IntervalTree::getBalanceFactor(const IntervalNode* node) const {
    return node ? getHeight(node->left.get()) - getHeight(node->right.get()) : 0;
}

std::unique_ptr<IntervalNode> IntervalTree::rotateRight(std::unique_ptr<IntervalNode> y) {
    std::cout << "Performing rotateRight on node with interval low: " << (y && y->interval ? std::to_string(y->interval->low) : "N/A") << std::endl;
    std::unique_ptr<IntervalNode> x = std::move(y->left);
    y->left = std::move(x->right);
    x->right = std::move(y);

    updateNodeHeight(x->right.get()); // Update height of y first
    updateNodeHeight(x.get());      // Then update height of x

    if (x->right) x->right->updateMaxHigh(); // Update max_high of y
    x->updateMaxHigh();                   // Update max_high of x

    return x;
}

std::unique_ptr<IntervalNode> IntervalTree::rotateLeft(std::unique_ptr<IntervalNode> x) {
    std::cout << "Performing rotateLeft on node with interval low: " << (x && x->interval ? std::to_string(x->interval->low) : "N/A") << std::endl;
    std::unique_ptr<IntervalNode> y = std::move(x->right);
    x->right = std::move(y->left);
    y->left = std::move(x);

    updateNodeHeight(y->left.get());  // Update height of x first
    updateNodeHeight(y.get());      // Then update height of y

    if (y->left) y->left->updateMaxHigh(); // Update max_high of x
    y->updateMaxHigh();                  // Update max_high of y

    return y;
}
