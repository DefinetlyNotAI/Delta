cmake_minimum_required(VERSION 3.20)

project(Delta C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(ARG WIN32
        worker.c
        state.h
)

add_executable(ARGLauncher WIN32
        launcher.c
)

target_link_libraries(ARG PRIVATE
        winmm
)

target_link_libraries(ARGLauncher PRIVATE
        user32
)