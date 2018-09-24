cmake_minimum_required(VERSION 3.12)
project(highload)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "-lrt")

add_executable(highload main.cpp declare.h HttpParser.cpp HttpParser.h MimeType.cpp MimeType.h)
target_compile_options (highload PRIVATE -lrt)
target_link_libraries (highload rt)