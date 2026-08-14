cmake_minimum_required(VERSION 3.20)

project(ARG C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(ARG WIN32
        main.c
)

target_link_libraries(ARG PRIVATE
        winmm
)