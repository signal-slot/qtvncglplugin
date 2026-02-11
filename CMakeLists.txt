cmake_minimum_required(VERSION 3.16)
project(qtvncglplugin VERSION 1.0 LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Network OpenGL)

add_subdirectory(src)
