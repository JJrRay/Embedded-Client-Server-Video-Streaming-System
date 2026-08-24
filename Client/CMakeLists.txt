cmake_minimum_required(VERSION 3.22)
project(prog CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


include_directories(
  ${PROJECT_SOURCE_DIR}/inc
  ${PROJECT_SOURCE_DIR}/src
)

file(GLOB all_SRCS
  "${PROJECT_SOURCE_DIR}/inc/*.h"
  "${PROJECT_SOURCE_DIR}/src/*.cpp"
)

add_executable(prog ${all_SRCS})

find_package(OpenCV REQUIRED)
target_include_directories(prog PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(prog PRIVATE ${OpenCV_LIBS})

find_package(Threads REQUIRED)
target_link_libraries(prog PRIVATE Threads::Threads stdc++fs)