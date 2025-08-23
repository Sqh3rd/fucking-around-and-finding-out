cmake_minimum_required(VERSION 4.0.0)
project(SCAn VERSION 0.1)

add_executable(${PROJECT_NAME} src/main.c)

target_link_libraries(${PROJECT_NAME} PRIVATE constants)
target_link_libraries(${PROJECT_NAME} PRIVATE logger)
target_link_libraries(${PROJECT_NAME} PRIVATE thread_pool)

add_subdirectory(src/constants)
add_subdirectory(src/logger)
add_subdirectory(src/thread_pool)