cmake_minimum_required(VERSION 3.12)
project(highload)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "-lrt -lpthread")

add_executable(highload main.cpp declare.h HttpParser.cpp HttpParser.h MimeType.cpp MimeType.h Server.cpp Server.h aoReadFile.cpp List/ListNode.cpp List/ConcurrentList.cpp parse/parse_config.h parse/parse_config.c)
target_compile_options (highload PRIVATE -lrt -lpthread)
target_link_libraries (highload rt pthread)