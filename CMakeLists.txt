cmake_minimum_required(VERSION 3.12)

set(NAME picovision-uc)

include(pimoroni_pico_import.cmake)
include(pico_sdk_import.cmake)

# Gooey boilerplate
project(${NAME} C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the SDK
pico_sdk_init()

# Add your source files
add_executable(${NAME}
    main.cpp # <-- Add source files here!
    usb_host.cpp
)

# Include required libraries
# This assumes `pimoroni-pico` is stored alongside your project
include(common/pimoroni_i2c)
include(common/pimoroni_bus)
include(drivers/dv_display/dv_display)
include(libraries/pico_graphics/pico_graphics)
include(drivers/fatfs/fatfs)
include(drivers/aps6404/aps6404)
include(libraries/jpegdec/jpegdec)
include(drivers/sdcard/sdcard)

# Don't forget to link the libraries you need!
target_link_libraries(${NAME}
    pimoroni_i2c
    pico_stdlib
    tinyusb_board
    tinyusb_host
    dv_display
    hardware_pio
    hardware_i2c
    hardware_uart
    pico_graphics
    fatfs
    jpegdec
    sdcard
)

target_include_directories(${NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_compile_definitions(${NAME} PRIVATE
    USB_HOST=1
    SDCARD_SPI_BUS=spi1
    SDCARD_PIN_SPI0_CS=15
    SDCARD_PIN_SPI0_SCK=10
    SDCARD_PIN_SPI0_MOSI=11
    SDCARD_PIN_SPI0_MISO=12
)

# create map/bin/hex file etc.
pico_add_extra_outputs(${NAME})

pico_enable_stdio_uart(${NAME} 1)
pico_enable_stdio_usb(${NAME} 0)

# Set up files for the release packages
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/${NAME}.uf2
    ${CMAKE_CURRENT_LIST_DIR}/README.md
    DESTINATION .
)

set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
set(CPACK_GENERATOR "ZIP" "TGZ")
include(CPack)
