cmake_minimum_required(VERSION 3.22)

set(APP_NAME app)
set(PICO_BOARD pico_w)

# SDK must be included before `project` definition
include(/home/zane/rasp-pico/wifi/pico-sdk/external/pico_sdk_import.cmake)
include(/home/zane/rasp-pico/wifi/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

project(${APP_NAME} C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if (PICO_SDK_VERSION_STRING VERSION_LESS "2.0.0")
    message(FATAL_ERROR "Raspberry Pi Pico SDK version 2.0.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

# If you want debug output from USB (pass -DPICO_STDIO_USB=1) this ensures you don't lose any debug output while USB is set up
if (NOT DEFINED PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS)
    set(PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS 3000)
endif()

# Initialize the SDK
pico_sdk_init()

add_compile_options(-Wall
    -Wno-format          # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
    -Wno-unused-function # we have some for the docs that aren't called
)

if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wno-maybe-uninitialized)
endif()

# ------

if (NOT TARGET pico_cyw43_arch)
    message(SEND_ERROR "Pico W support is not available")
endif()

set(WIFI_SSID $ENV{WIFI_SSID} CACHE INTERNAL "WiFi SSID for examples")
set(WIFI_PASSWORD $ENV{WIFI_PASSWORD} CACHE INTERNAL "WiFi password for examples")

# ------

# include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

# ------

add_executable(${APP_NAME}
    main.c
    ir_recv.c
    ir_send.c
    scd40.c
    cmd_gen.c
    ${PICO_LWIP_CONTRIB_PATH}/apps/ping/ping.c
)

# Enable USB and UART
pico_enable_stdio_usb(${APP_NAME} 1)
pico_enable_stdio_uart(${APP_NAME} 1)

pico_add_extra_outputs(${APP_NAME})

target_compile_definitions(${APP_NAME} PRIVATE
    WIFI_SSID=\"${WIFI_SSID}\"
    WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
    PING_USE_SOCKETS=1
    PICO_ENTER_USB_BOOT_ON_EXIT=1   # When the executable ends, it waits to have a new binary written to it
)

# 
target_compile_definitions(app PRIVATE )

target_include_directories(${APP_NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/.. # for our common FreeRTOSConfig
    ${CMAKE_CURRENT_LIST_DIR}/../.. # for our common lwipopts
    ${PICO_LWIP_CONTRIB_PATH}/apps/ping
)

target_link_libraries(${APP_NAME}
    pico_cyw43_arch_lwip_sys_freertos
    pico_stdlib
    hardware_pwm
    hardware_i2c
    pico_lwip_iperf
    FreeRTOS-Kernel-Heap4 # FreeRTOS kernel and dynamic heap
)