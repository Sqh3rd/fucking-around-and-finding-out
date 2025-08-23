add_library(thread_pool thread_pool.c thread_pool.h task_queue.c task_queue.h constants.h)

target_link_libraries(thread_pool PRIVATE logger)

target_include_directories(thread_pool
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)