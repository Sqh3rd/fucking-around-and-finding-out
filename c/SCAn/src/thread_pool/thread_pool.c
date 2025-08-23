#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "thread_pool.h"
#include "task_queue.h"

thread_pool_t *create_thread_pool(unsigned short thread_count, thread_pool_creation_status_t *status);
void free_pool(thread_pool_t *pool);

void free_pool(thread_pool_t *pool)
{
    if (!pool)
    {
        return;
    }

    // Worker Pool
    destroy_task_queue(pool->worker_pool->task_queue);
    free(pool->worker_pool->c_task_queue);
    free(pool->worker_pool->m_task_queue);

    free(pool->worker_pool->busy_threads);
    free(pool->worker_pool->c_busy_threads);
    free(pool->worker_pool->m_busy_threads);

    for (int i = 0; i < pool->worker_pool->worker_count; i++)
    {
        if (!pool->worker_pool->worker_threads[i])
        {
            continue;
        }
        pthread_cancel(*(pool->worker_pool->worker_threads[i]));
        free(pool->worker_pool->worker_threads[i]);
    }
    free(pool->worker_pool->worker_threads);

    free(pool->worker_pool);

    // Others
    pthread_cancel(*(pool->watcher_thread));
    free(pool->watcher_thread);

    pthread_cancel(*(pool->logger_thread));
    free(pool->logger_thread);

    free(pool);
}

typedef struct
{
    int id;
    worker_pool_t *worker_pool;
} worker_thread_entry_arg_t;

void *get_tasks(void *arg)
{
    worker_thread_entry_arg_t *thread_entry_arg = (worker_thread_entry_arg_t *)arg;
    worker_pool_t *worker_pool = thread_entry_arg->worker_pool;

    int id = thread_entry_arg->id;

    for (;;)
    {
        pthread_mutex_lock(worker_pool->m_task_queue);
        while (worker_pool->task_queue && worker_pool->task_queue->count == 0)
        {
            pthread_cond_wait(worker_pool->c_task_queue, worker_pool->m_task_queue);
        }

        if (worker_pool->task_queue == NULL)
        {
            pthread_mutex_unlock(worker_pool->m_task_queue);
            break;
        }

        int (*task_func)(task_queue_entry_arg_t *);
        task_queue_entry_arg_t *task_arg;

        int err = dequeue_task(worker_pool->task_queue, &task_func, &task_arg);
        if (err != 0)
        {
            pthread_mutex_unlock(worker_pool->m_task_queue);
            printf("%s Failed to dequeue task: %d\n", ERR, err);
            break;
        }
        if (task_func && task_arg)
        {
            pthread_mutex_unlock(worker_pool->m_task_queue);
            pthread_mutex_lock(worker_pool->m_busy_threads);
            *worker_pool->busy_threads |= 1 << id;
            pthread_cond_broadcast(worker_pool->c_busy_threads);
            pthread_mutex_unlock(worker_pool->m_busy_threads);

            task_arg->id = id;

            int err = task_func(task_arg);
            if (err)
            {
                printf("%s Task function failed with error: %d\n%s Terminating thread...\n", ERR, err, ERR);
                break;
            }

            pthread_mutex_lock(worker_pool->m_busy_threads);
            *worker_pool->busy_threads ^= 1 << id;
            pthread_cond_broadcast(worker_pool->c_busy_threads);
            pthread_mutex_unlock(worker_pool->m_busy_threads);
        }
    }
    printf("%s Thread %d finished\n", DEBUG, pthread_self());
}

void *watch_threads(void *arg)
{
    worker_pool_t *worker_pool = (worker_pool_t *)arg;

    int busy_threads_count;
    printf("%s Watcher Thread, bitches\n", DEBUG);
    //printf("%s Busy Threads: 0\n%s Open Tasks: \033[s0", DEBUG, DEBUG);
    for (;;)
    {
        pthread_mutex_lock(worker_pool->m_busy_threads);
        pthread_cond_wait(worker_pool->c_busy_threads, worker_pool->m_busy_threads);

        busy_threads_count = 0;
        for (int i = 0; i < sizeof(int) * 8; i++)
        {
            busy_threads_count += *worker_pool->busy_threads & 1 << i > 0 ? 1 : 0;
        }

        pthread_mutex_unlock(worker_pool->m_busy_threads);

        pthread_mutex_lock(worker_pool->m_task_queue);
        //printf("\033[F\033[22G %d", busy_threads_count);
        //printf("\033[u\033[K\033[u%d", worker_pool->task_queue->count);

        if (busy_threads_count == 0 && worker_pool->task_queue->count == 0)
        {
            printf("\n%s All tasks completed, destroying task queue...\n", DEBUG);
            int err = destroy_task_queue(worker_pool->task_queue);
            if (err)
            {
                printf("%s Failed to destroy task queue: %d\n", ERR, err);
            }
            worker_pool->task_queue = NULL;
            pthread_cond_broadcast(worker_pool->c_task_queue);
            pthread_mutex_unlock(worker_pool->m_task_queue);
            break;
        }
        pthread_mutex_unlock(worker_pool->m_task_queue);
    }
    printf("%s Watcher Thread exiting...\n", DEBUG);
}

thread_pool_t *create_thread_pool(unsigned short size, thread_pool_creation_status_t *status)
{
    *status = MAX_THREAD_AMOUNT_EXCEEDED;
    if (size <= 0 || size >= sizeof(unsigned int) * 8)
    {
        return NULL;
    }

    // --
    // -- MALLOC
    // --

    *status = OUT_OF_MEMORY;
    task_queue_t *task_queue = create_task_queue();
    if (!task_queue)
    {
        return NULL;
    }

    pthread_t **worker_threads = malloc(size * sizeof(pthread_t *));
    if (!worker_threads)
    {
        destroy_task_queue(task_queue);
        return NULL;
    }

    thread_pool_t *pool = malloc(sizeof(thread_pool_t));
    worker_pool_t *worker_pool = malloc(sizeof(worker_pool_t));
    pthread_t *watcher_thread = malloc(sizeof(pthread_t));
    pthread_t *logger_thread = malloc(sizeof(pthread_t));
    pthread_cond_t *c_busy_threads = malloc(sizeof(pthread_cond_t));
    pthread_cond_t *c_task_queue = malloc(sizeof(pthread_cond_t));
    pthread_mutex_t *m_busy_threads = malloc(sizeof(pthread_mutex_t));
    pthread_mutex_t *m_task_queue = malloc(sizeof(pthread_mutex_t));
    unsigned int *busy_threads = malloc(sizeof(unsigned int));

    unsigned short all_workers_mallocd = 1;
    for (int i = 0; i < size; i++)
    {
        worker_threads[i] = malloc(sizeof(pthread_t));
        if (!worker_threads[i])
        {
            all_workers_mallocd = 0;
            break;
        }
    }

    if (
        !pool ||
        !worker_pool ||
        !watcher_thread ||
        !logger_thread ||
        !c_busy_threads ||
        !c_task_queue ||
        !m_busy_threads ||
        !m_task_queue ||
        !busy_threads ||
        !all_workers_mallocd)
    {
        destroy_task_queue(task_queue);
        if (pool)
            free(pool);
        if (worker_pool)
            free(worker_pool);
        if (watcher_thread)
            free(watcher_thread);
        if (logger_thread)
            free(logger_thread);
        if (c_busy_threads)
            free(c_busy_threads);
        if (c_task_queue)
            free(c_task_queue);
        if (m_busy_threads)
            free(m_busy_threads);
        if (m_task_queue)
            free(m_task_queue);
        if (busy_threads)
            free(busy_threads);
        for (int i = 0; i < size; i++)
        {
            if (worker_threads[i])
                free(worker_threads[i]);
        }
        return NULL;
    }
    *busy_threads = 0;

    pool->logger_thread = logger_thread;
    pool->thread_count = size + 2;
    pool->watcher_thread = watcher_thread;
    pool->worker_pool = worker_pool;

    worker_pool->busy_threads = busy_threads;
    worker_pool->c_task_queue = c_task_queue;
    worker_pool->m_task_queue = m_task_queue;
    worker_pool->c_busy_threads = c_busy_threads;
    worker_pool->m_busy_threads = m_busy_threads;
    worker_pool->task_queue = task_queue;
    worker_pool->worker_count = size;
    worker_pool->worker_threads = worker_threads;

    // --
    // -- CREATE THREADS
    // --
    *status = THREAD_CREATION_FAILED;

    if (
        pthread_cond_init(c_busy_threads, NULL) ||
        pthread_cond_init(c_task_queue, NULL) ||
        pthread_mutex_init(m_busy_threads, NULL) ||
        pthread_mutex_init(m_task_queue, NULL))
    {
        free_pool(pool);
        return NULL;
    }

    for (int i = 0; i < size; i++)
    {
        worker_thread_entry_arg_t *thread_entry_arg = malloc(sizeof(worker_thread_entry_arg_t));
        if (!thread_entry_arg)
        {
            *status = OUT_OF_MEMORY;
            free_pool(pool);
            return NULL;
        }
        thread_entry_arg->worker_pool = worker_pool;
        thread_entry_arg->id = i;
        if (pthread_create(worker_threads[i], NULL, get_tasks, thread_entry_arg))
        {
            free_pool(pool);
            return NULL;
        }
    }

    if (pthread_create(watcher_thread, NULL, watch_threads, worker_pool))
    {
        free_pool(pool);
        return NULL;
    }

    *status = CREATED;

    printf("%s Thread Pool created with %d threads\n", DEBUG, size);

    return pool;
}