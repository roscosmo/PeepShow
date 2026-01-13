cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME PeepShow)

# Include toolchain file
include("cmake/gcc-arm-none-eabi.cmake")

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    Core/Src/ADP5360.c
    Core/Src/debug_uart.c
    Core/Src/rtos_isr_bridge.c
    Core/Src/TMAG5273.c
    Core/Src/TMAG_joy.c
    Core/Src/LS013B7DH05.c
    Core/Src/font8x8_basic.c
    Core/Src/display_task.c
    Core/Src/display_renderer.c
    Core/Src/render_demo.c
    Core/Src/ui_router.c
    Core/Src/ui_menu.c
    Core/Src/ui_actions.c
    Core/Src/ui_task.c
    Core/Src/page_menu.c
    Core/Src/page_home.c
    Core/Src/page_joy_cal.c
    Core/Src/page_joy_target.c
    Core/Src/page_joy_cursor.c
    Core/Src/page_sound.c
    Core/Src/page_menu_input.c
    Core/Src/page_batt_stats.c
    Core/Src/page_sleep.c
    Core/Src/page_storage.c
    Core/Src/settings.c
    Core/Src/game_task.c
    Core/Src/sensor_task.c
    Core/Src/audio_task.c
    Core/Src/storage_task.c
    Core/Src/power_task.c
    Core/Src/lfs.c
    Core/Src/lfs_util.c
)

# Speed up hot render paths.
set_source_files_properties(Core/Src/render_demo.c PROPERTIES
    COMPILE_FLAGS "-O3 -ffast-math -fno-math-errno -ffp-contract=fast")
set_source_files_properties(Core/Src/display_renderer.c PROPERTIES
    COMPILE_FLAGS "-O3 -ffast-math -fno-math-errno -ffp-contract=fast")

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
    LFS_NO_MALLOC
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
