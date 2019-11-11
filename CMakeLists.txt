project(sup CXX)

cmake_minimum_required(VERSION 2.8)
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake_modules")
message(STATUS "CMAKE_MODULE_PATH: ${CMAKE_MODULE_PATH}")

## Set CXX optimization flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fext-numeric-literals -Wpedantic -Wall -Wextra")
## -mtune=native -march=native

## Set build type to Release by default
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")
endif(NOT CMAKE_BUILD_TYPE)
message(STATUS "Build type: " ${CMAKE_BUILD_TYPE})

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
else(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0")
endif(CMAKE_BUILD_TYPE STREQUAL "Release")
message(STATUS "Flags: " ${CMAKE_CXX_FLAGS})

## Dependencies
find_package(Boost REQUIRED filesystem system)

find_package(Git)
include(GenerateVersionDefinitions)

include_directories(include ${Boost_INCLUDE_DIRS})

add_executable(ass_split src/ass_split.cpp src/string.cpp)
target_link_libraries(ass_split ${Boost_LIBRARIES})

add_executable(ass_merge src/ass_merge.cpp src/string.cpp)
target_link_libraries(ass_merge ${Boost_LIBRARIES})

add_executable(ass_extract src/ass_extract.cpp src/string.cpp)
target_link_libraries(ass_extract ${Boost_LIBRARIES})

add_executable(ass_sort src/ass_sort.cpp src/string.cpp)
target_link_libraries(ass_sort ${Boost_LIBRARIES})

add_executable(ass_time src/ass_time.cpp src/string.cpp)
target_link_libraries(ass_time ${Boost_LIBRARIES})
