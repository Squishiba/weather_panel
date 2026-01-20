# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()

set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)

if(EXISTS ${picoVscode})
    include(${picoVscode})
endif()

# ====================================================================================
set(PIMORONI_PICO_PATH ./pimoroni-pico)
set(PICO_PLATFORM rp2350-arm-s)
set(PICO_BOARD pico2_w)
set(PICO_COMPILER)

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pimoroni_pico_import.cmake)
include(pico_sdk_import.cmake)

# Pull in CMSIS-DSP
set(CMSISCORE ${PICO_SDK_PATH}/src/rp2_common/cmsis/stub/CMSIS/Core)

set(DISABLEFLOAT16 ON)
set(LOOPUNROLL ON)
include(FetchContent)
FetchContent_Declare(cmsisdsp
    GIT_REPOSITORY https://github.com/ARM-software/CMSIS-DSP.git
    GIT_TAG "v1.16.2"
)
FetchContent_MakeAvailable(cmsisdsp)

FetchContent_Declare(gcem
    GIT_REPOSITORY https://github.com/kthohr/gcem.git
    GIT_TAG "v1.18.0"
)

FetchContent_MakeAvailable(gcem)

project(weather C CXX ASM)

pico_sdk_init()

# Include required libraries
# This assumes `pimoroni-pico` is stored alongside your project
include(common/pimoroni_i2c)
include(common/pimoroni_bus)
include(libraries/pico_graphics/pico_graphics)
include(libraries/pico_vector/pico_vector)
include(libraries/interstate75/interstate75)

add_executable(weather src/main.cpp)

set(CMAKE_LINK_WHAT_YOU_USE TRUE)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

pico_enable_stdio_usb(weather 1)
pico_set_program_name(weather "weather")
pico_set_program_version(weather "1.0")

add_subdirectory(src)

add_dependencies(weather
    CMSISDSP
    gcem)

# Add the standard library to the build
target_link_libraries(weather
    ${cmsisdsp_BINARY_DIR}/Source/libCMSISDSP.a
    pico_stdlib
    hub75
    interstate75
    pico_multicore
        pico_sync
    pico_rand
    pimoroni_i2c
    pico_graphics
    hardware_timer
    hardware_clocks
    hardware_pwm
)

# Add the standard include files to the build
target_include_directories(weather PRIVATE
    ${cmsisdsp_SOURCE_DIR}/Include
    ${PICO_SDK_PATH}/src/rp2_common/cmsis/stub/CMSIS/Core/Include
    ${gcem_SOURCE_DIR}/Include
    ${CMAKE_CURRENT_LIST_DIR}/inc
)

add_compile_options(-Wall
    -Wno-format # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
    -Wno-unused-function
    -Wno-maybe-uninitialized
    -funroll-loops
)

# Enable ARM DSP/SIMD extensions for Cortex-M33
target_compile_definitions(weather PRIVATE
    ARM_MATH_CM33
    __ARM_FEATURE_DSP=1
)

pico_add_extra_outputs(weather)
