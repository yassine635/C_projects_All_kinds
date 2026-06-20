cmake_minimum_required(VERSION 3.10)
project(MyRaylibGame C)

# Find Raylib on your Fedora system
find_package(raylib REQUIRED)

add_executable(MyGame main.c)

# Tell the compiler to link Raylib to your game
target_link_libraries(MyGame PRIVATE raylib)