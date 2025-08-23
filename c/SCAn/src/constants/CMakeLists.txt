add_library(constants constants.c constants.h)

target_include_directories(constants
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
