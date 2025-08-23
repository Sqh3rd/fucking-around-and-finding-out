add_library(thread_pool thread_pool.c thread_pool.h task_queue.c task_queue.h)

target_include_directories(thread_pool
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)