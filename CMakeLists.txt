cmake_minimum_required(VERSION 3.12)
project(PacketClassifierEngine LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF) # Disable GNU extensions if specific standard conformance is needed

# --- Library Target (packet_classifier_lib) ---
add_library(packet_classifier_lib STATIC 
    # Core classifier
    src/packet_classifier.cpp

    # Data structures
    src/data_structures/compressed_trie.cpp
    src/data_structures/concurrent_hash.cpp
    src/data_structures/interval_tree.cpp
    src/data_structures/bloom_filter.cpp

    # Utilities
    src/utils/memory_pool.cpp
    src/utils/threading.cpp
    src/utils/logging.cpp
    src/utils/rule_manager.cpp
)

# Specify include directories for the library and for targets linking against it
target_include_directories(packet_classifier_lib PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    # No need to add subdirectories of include if they are referenced like "data_structures/common.h"
    # If includes are like #include "common.h", then add:
    # ${CMAKE_CURRENT_SOURCE_DIR}/include/data_structures 
    # ${CMAKE_CURRENT_SOURCE_DIR}/include/utils
)

# --- Threads ---
find_package(Threads REQUIRED)
target_link_libraries(packet_classifier_lib PRIVATE Threads::Threads)


# --- Example Executable Target (classifier_example) ---
add_executable(classifier_example
    examples/main.cpp
)

# Link the example against the library
target_link_libraries(classifier_example PRIVATE packet_classifier_lib)

# Specify include directories for the example executable (if not already covered by PUBLIC in library)
# target_include_directories(classifier_example PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include) 
# This is typically handled by packet_classifier_lib's PUBLIC include directories.


# --- Optional: Installation (if this were a distributable library) ---
# install(TARGETS packet_classifier_lib
#     EXPORT packet_classifier_lib_targets
#     LIBRARY DESTINATION lib
#     ARCHIVE DESTINATION lib
#     RUNTIME DESTINATION bin
#     INCLUDES DESTINATION include
# )
# install(DIRECTORY include/ DESTINATION include)
# install(EXPORT packet_classifier_lib_targets
#   FILE PacketClassifierLibTargets.cmake
#   NAMESPACE PacketClassifierLib::
#   DESTINATION lib/cmake/PacketClassifierLib
# )

# --- Optional: Testing (e.g., with CTest) ---
# enable_testing()
# add_test(NAME example_runs
#     COMMAND classifier_example
# )
# Example: add_executable(run_tests tests/test_runner.cpp)
# target_link_libraries(run_tests PRIVATE packet_classifier_lib Threads::Threads)
# add_test(NAME unit_tests COMMAND run_tests)


# --- Output ---
message(STATUS "CMake Project: ${PROJECT_NAME}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}") # Often empty initially, set by user e.g. -DCMAKE_BUILD_TYPE=Release
message(STATUS "Generator: ${CMAKE_GENERATOR}")

# To build:
# mkdir build
# cd build
# cmake ..
# cmake --build . 
# or
# make
# ./examples/classifier_example (or ./classifier_example if executable is in build root)
