# CMake generated Testfile for 
# Source directory: /Users/ikovic/Documents/sqlgui/build/_deps/tl_expected-src
# Build directory: /Users/ikovic/Documents/sqlgui/build/_deps/tl_expected-build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(tl::expected::tests "/Users/ikovic/Documents/sqlgui/build/_deps/tl_expected-build/tl-expected-tests")
set_tests_properties(tl::expected::tests PROPERTIES  _BACKTRACE_TRIPLES "/Users/ikovic/Documents/sqlgui/build/_deps/tl_expected-src/CMakeLists.txt;84;add_test;/Users/ikovic/Documents/sqlgui/build/_deps/tl_expected-src/CMakeLists.txt;0;")
subdirs("../catch2-build")
