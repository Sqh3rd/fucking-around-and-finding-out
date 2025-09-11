add_library(cat cat.c cat.h)

target_include_directories(cat
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)