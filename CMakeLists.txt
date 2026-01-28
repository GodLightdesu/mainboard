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
set(CMAKE_PROJECT_NAME mainboard)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

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
    Core/Src/main.c
    Libraries/I2C/i2c_common.c
    Libraries/Uart/data_uart.c
    Libraries/Module/mpu6050.c
    Libraries/Module/mpu6050_dmp.c
    Libraries/Module/motors.c
    Libraries/Module/ir.c
    Libraries/soccer.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE

    # Add user defined include paths
    Core/Inc
    Libraries/Module
    Libraries/Uart
    Libraries/I2C
    Libraries/
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE

    # Add user defined symbols
    # Uncomment the lines below to enable debug UART printing for each module
    DEBUG_GENERAL # General debug messages

    # DEBUG_MPU6050        # MPU6050 sensor data and initialization
    # DEBUG_MPU6050_DMP    # MPU6050 attitude (Roll/Pitch/Yaw)
    DEBUG_IR # IR sensor eye data

    # DEBUG_I2C            # I2C communication errors and status
    # DEBUG_MOTORS         # Motor testing PWM values
    # DEBUG_SOCCER         # Soccer state machine transitions and decisions
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)

# Enable float support in printf (requires more flash/RAM)
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    -Wl,-u,_printf_float
)
