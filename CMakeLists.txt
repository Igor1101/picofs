cmake_minimum_required(VERSION 3.14)

project(picofs LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
add_definitions(-D_CRT_SECURE_NO_WARNINGS)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5Core)

add_executable(picofs
  main.cpp picofs.cpp picofs.h picofs_cli.cpp picofs_cli.h
)
target_link_libraries(picofs)
