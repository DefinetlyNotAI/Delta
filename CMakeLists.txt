cmake_minimum_required(VERSION 3.20)

project(Delta C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(ARG WIN32
        worker.c
        state.h
)

target_link_libraries(ARG PRIVATE
        winmm
        ole32
        sapi
        crypt32
        shell32
        gdiplus
)

add_executable(TREE WIN32
        tree.c
        tree.rc
)

target_link_libraries(TREE PRIVATE
        gdiplus
        ole32
        shell32
)
