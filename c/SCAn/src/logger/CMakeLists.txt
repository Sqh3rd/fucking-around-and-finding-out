add_library(logger logger.c logger.h)

target_link_libraries(logger PRIVATE constants)

target_include_directories(logger
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)