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
    PRIV_REQUIRES nofrendo-esp32
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -std=gnu17
    -w
    -DNOFRENDO_DEBUG
)