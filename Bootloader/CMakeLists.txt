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
set(CMAKE_PROJECT_NAME Bootloader)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME}
    BSP/Inc/bsp_w25qxx.h
    BSP/Inc/bsp_at24cxx.h
    BSP/Inc/bsp_flash.h
    Ota/Inc/ota_boot.h
    Ota/Inc/ota_protocol.h
    Ota/Inc/ota_config.h
    Utils/Inc/crc32.h
    BSP/Inc/OLED.h
    BSP/Inc/bsp_ascii_font.h
    Core/Inc/boot_config.h
    Core/Inc/boot_jump.h)
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE
    -Os -g0 -ffunction-sections -fdata-sections -fno-common -fno-builtin
)
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    -Wl,--gc-sections -Wl,-Map=${CMAKE_PROJECT_NAME}.map
)

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    BSP/Src/bsp_w25qxx.c
    BSP/Src/bsp_at24cxx.c
    BSP/Src/bsp_flash.c
    BSP/Src/OLED.c
    Ota/Src/ota_boot.c
    Utils/Src/crc32.c
    Core/Src/boot_jump.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    BSP/Inc
    Ota/Inc
    Utils/Inc
    Core/Inc
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)