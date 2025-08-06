cmake_minimum_required(VERSION 4.0.0)
project(CLI_Calculator VERSION 0.1)

add_executable(${PROJECT_NAME} src/main.c)

target_link_libraries(${PROJECT_NAME} expression)
target_link_libraries(${PROJECT_NAME} flags)

add_subdirectory(src/expression)
add_subdirectory(src/flags)