add_library(expression expression.h calculate.c parse.c print.c structs.c util.c validate.c)

target_include_directories(expression
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)