#include <stdio.h>
#include <dirent.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <pthread.h>

#include "thread_pool.h"
#include "logger.h"

pthread_mutex_t m_files = PTHREAD_MUTEX_INITIALIZER;

thread_pool_t *thread_pool;
int *counters;

const int DEFAULT_THREAD_COUNT = 5;

typedef struct directory_name_t
{
        char *name;
        int name_len;
} directory_name_t;

typedef struct file_entry_t file_entry_t;
typedef struct file_entry_t
{
        char *name;
        int name_len;
        file_entry_t *next;
} file_entry_t;

typedef struct file_list_t {
        file_entry_t *first;
        file_entry_t *last;
} file_list_t;

typedef struct file_ending_entry_t {
        char *ending;
        int ending_len;
        file_entry_t *first;
        file_entry_t *last;
} file_ending_entry_t;

file_list_t *files;

int traverse_directories(task_queue_entry_arg_t *task_arg)
{
        directory_name_t *dir_name = (directory_name_t *)(task_arg->arg);
        DIR *pDir;

        pDir = opendir(dir_name->name);
        if (pDir == NULL)
        {
                return 1;
        }

        counters[2 * task_arg->id]++;

        log_debug("Traverse: %s\n", dir_name->name);

        struct dirent *pDirent;
        while ((pDirent = readdir(pDir)) != NULL)
        {
                if (!strcmp(".", pDirent->d_name) ||
                    !strcmp("..", pDirent->d_name) ||
                    !strcmp(".scan", pDirent->d_name) ||
                    !strcmp(".git", pDirent->d_name) ||
                    !strcmp(".idea", pDirent->d_name)
                    //!strcmp("node_modules", pDirent->d_name)
                )
                        continue;

                int sub_dir_name_length = dir_name->name_len + 2 + pDirent->d_namlen;
                char *sub_dir_name = malloc(sub_dir_name_length);

                strcpy(sub_dir_name, dir_name->name);
                strcat(sub_dir_name, "/");
                strcat(sub_dir_name, pDirent->d_name);

                struct stat *s = malloc(sizeof(struct stat));
                if (stat(sub_dir_name, s) != 0)
                {
                        free(s);
                        free(sub_dir_name);
                        continue;
                }

                if (s->st_mode & S_IFDIR)
                {
                        task_queue_entry_arg_t *next_arg = malloc(sizeof(task_queue_entry_arg_t));
                        directory_name_t *next = malloc(sizeof(directory_name_t));
                        next->name = sub_dir_name;
                        next->name_len = sub_dir_name_length;
                        next_arg->arg = next;

                        log_debug("Enqueueing directory: %s\n", sub_dir_name);
                        int err = enqueue_task(thread_pool, traverse_directories, next_arg);
                        if (err != 0)
                        {
                                free(next->name);
                                free(next);
                                free(s);
                                free(sub_dir_name);
                                log_error("Failed to enqueue task for directory: %s\n", sub_dir_name);
                                return err;
                        }
                }
                else if (s->st_mode & S_IFREG)
                {
                        counters[2 * task_arg->id + 1]++;
                        file_entry_t *file = malloc(sizeof(file_entry_t));
                        file->name = sub_dir_name;
                        file->name_len = sub_dir_name_length;
                        file->next = NULL;

                        pthread_mutex_lock(&m_files);
                        if (files->first == NULL)
                        {
                                files->first = file;
                                files->last = file;
                        }
                        else
                        {
                                files->last->next = file;
                                files->last = file;
                        }
                        pthread_mutex_unlock(&m_files);
                }
                free(s);
        }
        closedir(pDir);
        free(dir_name->name);
        free(dir_name);
        free(task_arg);
        return 0;
}

int traverse(int length, char *path)
{
        directory_name_t *dir_name = malloc(sizeof(directory_name_t));
        dir_name->name = path;
        dir_name->name_len = length;

        task_queue_entry_arg_t *arg = malloc(sizeof(task_queue_entry_arg_t));
        arg->arg = dir_name;

        enqueue_task(thread_pool, traverse_directories, arg);

        return 0;
}

int printd(char *str, const time_t *time)
{
        char buff[30];
        struct tm *timeinfo = localtime(time);
        strftime(buff, sizeof(buff), "%Y:%m:%d %H:%M:%S", timeinfo);

        log_info("%s %s\n", str, buff);
}


int main(int argc, char *argv[])
{
        if (argc > 2)
        {
                printf("Usage: SCAn [dirname]\n");
                return 1;
        }

        init_logger(LOG_LEVEL_INFO);

        char *path;
        if (argc == 2)
        {
                path = argv[1];
        }
        else
        {
                path = malloc(2);
                *path = '.';
                *(path + 1) = '\0';
        }

        struct stat s;
        stat(path, &s);
        printd("Last top level status change:", &(s.st_ctime));
        printd("Last top level data change:  ", &(s.st_mtime));

        counters = malloc(2 * DEFAULT_THREAD_COUNT * sizeof(int));
        for (int i = 0; i < DEFAULT_THREAD_COUNT; i++)
        {
                counters[2 * i] = 0;
                counters[2 * i + 1] = 0;
        }

        files = malloc(sizeof(file_list_t));
        files->first = NULL;
        files->last = NULL;

        thread_pool_creation_status_t *status = malloc(sizeof(thread_pool_creation_status_t));
        thread_pool = create_thread_pool(DEFAULT_THREAD_COUNT, status);

        if (status == NULL || *status != CREATED)
        {
                return 1;
        }

        clock_t begin = clock();

        if (traverse(strlen(path), path))
        {
                return 2;
        }

        join(thread_pool);

        clock_t end = clock();
        int directories = 0;
        int files_amount = 0;
        for (int i = 0; i < DEFAULT_THREAD_COUNT; i++)
        {
                directories += *(counters + 2 * i);
                files_amount += *(counters + 2 * i + 1);
        }

        double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
        log_info("Traversed %d directories and found %d files in %fs\n", directories, files_amount, time_spent);
        stop_logger();
        return 0;
}
