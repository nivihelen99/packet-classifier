#include "packet_classifier.h" // Assuming this path will be correct once CMake sets include directories
#include <iostream>

// Minimal main for testing the build system and library linkage.
int main(int argc, char* argv[]) {
    std::cout << "Attempting to create PacketClassifier instance..." << std::endl;
    try {
        PacketClassifier classifier(true); // Enable Bloom filter for this example
        std::cout << "PacketClassifier instance created successfully." << std::endl;

        // Example: Add a simple rule to test addRule basic functionality
        // Note: PacketFilter and ActionList would need to be populated for a meaningful rule.
        // This is just to ensure the addRule method can be called.
        PacketFilter filter; 
        // filter.source_ip_prefix = "192.168.1.0/24"; // Example
        filter.protocol = 6; // TCP

        ActionList actions;
        actions.primary_action = ActionList::ActionType::FORWARD;
        actions.next_hop_id = 10;
        
        ClassificationRule rule(1, 100, filter, actions); // id, priority, filter, actions
        rule.enabled = true;

        std::cout << "Attempting to add a dummy rule..." << std::endl;
        if (classifier.addRule(rule)) {
            std::cout << "Dummy rule added successfully." << std::endl;
        } else {
            std::cout << "Failed to add dummy rule." << std::endl;
        }

        // Example: Classify a dummy packet
        // PacketHeader packet(0x01020304, 0x05060708, 12345, 80, 6); // srcIP, dstIP, srcPort, dstPort, proto
        // std::cout << "Attempting to classify a dummy packet..." << std::endl;
        // ClassificationResult result = classifier.classify(packet);
        // std::cout << "Classification result: " << result.toString() << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Exception caught in main: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown exception caught in main." << std::endl;
        return 1;
    }

    std::cout << "Example program finished." << std::endl;
    return 0;
}
