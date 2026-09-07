# CMake generated Testfile for 
# Source directory: /Users/robertknigge/Downloads/triax
# Build directory: /Users/robertknigge/Downloads/triax/build-san
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(triax-c "/Users/robertknigge/Downloads/triax/build-san/triax-selftest-c" "--no-color")
set_tests_properties(triax-c PROPERTIES  _BACKTRACE_TRIPLES "/Users/robertknigge/Downloads/triax/CMakeLists.txt;315;add_test;/Users/robertknigge/Downloads/triax/CMakeLists.txt;0;")
add_test(triax-c-parallel "/Users/robertknigge/Downloads/triax/build-san/triax-selftest-c" "--no-color" "--jobs=2")
set_tests_properties(triax-c-parallel PROPERTIES  _BACKTRACE_TRIPLES "/Users/robertknigge/Downloads/triax/CMakeLists.txt;320;add_test;/Users/robertknigge/Downloads/triax/CMakeLists.txt;0;")
add_test(triax-cpp "/Users/robertknigge/Downloads/triax/build-san/triax-selftest-cpp" "--no-color")
set_tests_properties(triax-cpp PROPERTIES  _BACKTRACE_TRIPLES "/Users/robertknigge/Downloads/triax/CMakeLists.txt;325;add_test;/Users/robertknigge/Downloads/triax/CMakeLists.txt;0;")
add_test(triax-cpp-parallel "/Users/robertknigge/Downloads/triax/build-san/triax-selftest-cpp" "--no-color" "--jobs=2")
set_tests_properties(triax-cpp-parallel PROPERTIES  _BACKTRACE_TRIPLES "/Users/robertknigge/Downloads/triax/CMakeLists.txt;330;add_test;/Users/robertknigge/Downloads/triax/CMakeLists.txt;0;")
add_test(triax-c-formats "/bin/bash" "/Users/robertknigge/Downloads/triax/tests/validate_formats.sh" "/Users/robertknigge/Downloads/triax/build-san/triax-selftest-c" "exits")
set_tests_properties(triax-c-formats PROPERTIES  _BACKTRACE_TRIPLES "/Users/robertknigge/Downloads/triax/CMakeLists.txt;374;add_test;/Users/robertknigge/Downloads/triax/CMakeLists.txt;0;")
add_test(triax-cpp-formats "/bin/bash" "/Users/robertknigge/Downloads/triax/tests/validate_formats.sh" "/Users/robertknigge/Downloads/triax/build-san/triax-selftest-cpp" "cpp_fmt")
set_tests_properties(triax-cpp-formats PROPERTIES  _BACKTRACE_TRIPLES "/Users/robertknigge/Downloads/triax/CMakeLists.txt;383;add_test;/Users/robertknigge/Downloads/triax/CMakeLists.txt;0;")
add_test(triax-cli "/bin/bash" "/Users/robertknigge/Downloads/triax/tests/validate_cli.sh" "/Users/robertknigge/Downloads/triax/build-san/triax-cli-fixture")
set_tests_properties(triax-cli PROPERTIES  _BACKTRACE_TRIPLES "/Users/robertknigge/Downloads/triax/CMakeLists.txt;404;add_test;/Users/robertknigge/Downloads/triax/CMakeLists.txt;0;")
add_test(triax-combinations "/bin/bash" "/Users/robertknigge/Downloads/triax/tests/validate_combinations.sh" "/Users/robertknigge/Downloads/triax/build-san/triax-combo-fixture" "/Users/robertknigge/Downloads/triax/build-san/triax-combo-fixture-cpp" "/Users/robertknigge/Downloads/triax/build-san/triax-multitu-c" "/Users/robertknigge/Downloads/triax/build-san/triax-multitu-cpp")
set_tests_properties(triax-combinations PROPERTIES  _BACKTRACE_TRIPLES "/Users/robertknigge/Downloads/triax/CMakeLists.txt;454;add_test;/Users/robertknigge/Downloads/triax/CMakeLists.txt;0;")
