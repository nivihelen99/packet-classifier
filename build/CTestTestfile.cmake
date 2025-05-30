# CMake generated Testfile for 
# Source directory: /app
# Build directory: /app/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(UnitTests "/app/build/unit_tests_runner")
set_tests_properties(UnitTests PROPERTIES  _BACKTRACE_TRIPLES "/app/CMakeLists.txt;93;add_test;/app/CMakeLists.txt;0;")
