cmake_minimum_required(VERSION 3.20)

project(ARG C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(ARGLauncher WIN32
        launcher.c
        state.h
)

add_executable(ARGWorkerTemplate WIN32
        worker.c
        state.h
)

target_link_libraries(ARGWorkerTemplate PRIVATE
        winmm
)