cmake_minimum_required(VERSION 3.16)
project(qtvncglplugin VERSION 1.0.0 LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Network OpenGL)
find_package(Qt6 REQUIRED COMPONENTS CorePrivate GuiPrivate OpenGLPrivate)
find_package(EGL REQUIRED)
find_package(ZLIB REQUIRED)

set(CMAKE_AUTOMOC ON)

# Include Qt install paths (provides QT6_INSTALL_PLUGINS etc.)
include("${Qt6_DIR}/QtInstallPaths.cmake")

add_subdirectory(src)
