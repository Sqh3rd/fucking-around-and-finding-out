add_library(flags flags.h flags.c)

target_include_directories(flags
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)