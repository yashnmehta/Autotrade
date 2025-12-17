cmake_minimum_required(VERSION 3.10)
project(nse_udp_test)

set(CMAKE_CXX_STANDARD 17)

add_executable(nse_udp_test main.cpp minilzo/minilzo.c)
target_link_libraries(nse_udp_test pthread /lib/x86_64-linux-gnu/liblzo2.so.2)
