project(DI-light)
cmake_minimum_required(VERSION 2.8)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra -ggdb")
add_executable(${PROJECT_NAME} 
    test.cpp)

add_executable(hello
    helloworld.cpp)

