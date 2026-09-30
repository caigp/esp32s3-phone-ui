# components/nofrendo/CMakeLists.txt
file(GLOB_RECURSE NOFRENDO_SRCS
     "cpu/*.c"
     "libsnss/*.c"
     "nes/*.c"
     "sndhrdw/*.c"
     "mappers/*.c"
     "*.c"
)

idf_component_register(
    SRCS ${NOFRENDO_SRCS}
    INCLUDE_DIRS "." "cpu" "libsnss" "nes" "sndhrdw" "mappers"
    PRIV_REQUIRES nofrendo-esp32 esp_timer
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -std=gnu17
    # The emulator runs a complete 262-scanline frame on every tick.  Keep
    # this component optimized even when the application uses IDF's debug
    # optimization level; the video/audio path is timing-sensitive.
    -O2
    -w
    -DNOFRENDO_DEBUG
)
