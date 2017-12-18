cmake_minimum_required(VERSION 3.8)
project(RawsteelMusicPlayer)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_AUTOMOC ON)

find_package(Qt5Widgets REQUIRED)
find_package(Qt5Multimedia REQUIRED)

add_executable(RawsteelMusicPlayer main.cpp)

target_link_libraries(RawsteelMusicPlayer Qt5::Widgets Qt5::Multimedia)
