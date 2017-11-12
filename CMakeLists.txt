cmake_minimum_required(VERSION 2.8)

project(Qute)
set(Tutorial_VERSION_MAJOR 1)
set(Tutorial_VERSION_MINOR 0)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++11 -DBOOST_LOG_DYN_LINK")
if (CMAKE_BUILD_TYPE)
  message("Build type set to ${CMAKE_BUILD_TYPE}.")
else()
  set(CMAKE_BUILD_TYPE Release)
endif ()

set(Boost_USE_STATIC_LIBS OFF)

# On macOS + Homebrew, Boost only comes with libboost_thread-mt.a
# but no libboost_thread.a. Not setting Boost_USE_MULTITHREADED seems
# to fix the problem; see https://stackoverflow.com/a/15040393/3808537.
# set(Boost_USE_MULTITHREADED ON)

set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost 1.60.0 COMPONENTS program_options thread log system REQUIRED)

if (Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
else(Boost_FOUND)
  message(FATAL_ERROR "Qute requires Boost version 1.60 or above.")
endif (Boost_FOUND)

find_package(Threads REQUIRED)

add_subdirectory("src")
