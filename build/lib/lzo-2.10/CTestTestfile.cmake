# CMake generated Testfile for 
# Source directory: C:/Users/admin/Desktop/trading_terminal_cpp/lib/lzo-2.10
# Build directory: C:/Users/admin/Desktop/trading_terminal_cpp/build/lib/lzo-2.10
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(simple "C:/Users/admin/Desktop/trading_terminal_cpp/build/lib/lzo-2.10/simple.exe")
set_tests_properties(simple PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/admin/Desktop/trading_terminal_cpp/lib/lzo-2.10/CMakeLists.txt;254;add_test;C:/Users/admin/Desktop/trading_terminal_cpp/lib/lzo-2.10/CMakeLists.txt;0;")
add_test(testmini "C:/Users/admin/Desktop/trading_terminal_cpp/build/lib/lzo-2.10/testmini.exe")
set_tests_properties(testmini PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/admin/Desktop/trading_terminal_cpp/lib/lzo-2.10/CMakeLists.txt;255;add_test;C:/Users/admin/Desktop/trading_terminal_cpp/lib/lzo-2.10/CMakeLists.txt;0;")
add_test(lzotest-01 "C:/Users/admin/Desktop/trading_terminal_cpp/build/lib/lzo-2.10/lzotest.exe" "-mlzo" "-n2" "-q" "C:/Users/admin/Desktop/trading_terminal_cpp/lib/lzo-2.10/COPYING")
set_tests_properties(lzotest-01 PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/admin/Desktop/trading_terminal_cpp/lib/lzo-2.10/CMakeLists.txt;256;add_test;C:/Users/admin/Desktop/trading_terminal_cpp/lib/lzo-2.10/CMakeLists.txt;0;")
add_test(lzotest-02 "C:/Users/admin/Desktop/trading_terminal_cpp/build/lib/lzo-2.10/lzotest.exe" "-mavail" "-n10" "-q" "C:/Users/admin/Desktop/trading_terminal_cpp/lib/lzo-2.10/COPYING")
set_tests_properties(lzotest-02 PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/admin/Desktop/trading_terminal_cpp/lib/lzo-2.10/CMakeLists.txt;257;add_test;C:/Users/admin/Desktop/trading_terminal_cpp/lib/lzo-2.10/CMakeLists.txt;0;")
add_test(lzotest-03 "C:/Users/admin/Desktop/trading_terminal_cpp/build/lib/lzo-2.10/lzotest.exe" "-mall" "-n10" "-q" "C:/Users/admin/Desktop/trading_terminal_cpp/lib/lzo-2.10/include/lzo/lzodefs.h")
set_tests_properties(lzotest-03 PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/admin/Desktop/trading_terminal_cpp/lib/lzo-2.10/CMakeLists.txt;258;add_test;C:/Users/admin/Desktop/trading_terminal_cpp/lib/lzo-2.10/CMakeLists.txt;0;")
