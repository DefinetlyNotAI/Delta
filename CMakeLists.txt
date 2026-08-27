cmake_minimum_required(VERSION 3.20)

project(Delta LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(ARG WIN32
        src/state.h
        src/main.c
        src/arg_state.c
        src/arg_core.c
        src/arg_audio.c
        src/arg_overlay.c
        src/arg_text.c
        src/arg_step7.c
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
        src/tree.c
        src/tree.rc
)

target_link_libraries(TREE PRIVATE
        ole32
        shell32
        gdiplus
)

if (NOT MSVC)
    # MinGW needs this to select wWinMain for the Unicode GUI entry point.
    target_link_options(TREE PRIVATE -municode)
endif ()

foreach (target ARG TREE)
    target_compile_features(${target} PRIVATE c_std_11)

    if (MSVC)
        target_compile_options(${target} PRIVATE
                /W4
                /permissive-
        )
    else ()
        target_compile_options(${target} PRIVATE
                -Wall
                -Wextra
                -Wpedantic
        )
    endif ()
endforeach ()

target_compile_definitions(ARG PRIVATE
        WIN32_LEAN_AND_MEAN
        COBJMACROS
)

target_compile_definitions(TREE PRIVATE
        WIN32_LEAN_AND_MEAN
        COBJMACROS
        UNICODE
        _UNICODE
)
