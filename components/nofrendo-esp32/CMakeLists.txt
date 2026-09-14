# components/nofrendo/CMakeLists.txt
file(GLOB_RECURSE NOFRENDO_ESP32_SRCS
     "*.c"
)

idf_component_register(
    SRCS ${NOFRENDO_ESP32_SRCS}
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_timer freertos nofrendo esp_driver_gpio
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -std=gnu17
    -w
)