#include <stdio.h>
#include <dirent.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <ftw.h>

int *counter;

int traverse_directories(int dir_name_length, char *dir_name)
{
        DIR *pDir;

        // Ensure we can open directory.

        pDir = opendir(dir_name);
        if (pDir == NULL)
        {
                return 1;
        }

        *counter = *counter + 1;

        struct dirent *pDirent;
        while ((pDirent = readdir(pDir)) != NULL)
        {
                if (!strcmp(".", pDirent->d_name) || !strcmp("..", pDirent->d_name || !strcmp(".scan", pDirent->d_name)))
                        continue;

                int sub_dir_name_length = dir_name_length + 2 + pDirent->d_namlen;
                char *sub_dir_name = malloc(sub_dir_name_length);

                strcpy(sub_dir_name, dir_name);
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
                        traverse_directories(sub_dir_name_length, sub_dir_name);
                }
                else if (s->st_mode & S_IFREG)
                {
                        *(counter + 1) = *(counter + 1) + 1;
                }
                free(s);
                free(sub_dir_name);
        }

        closedir(pDir);
        return 0;
}

int printd(char *str, const time_t *time) {
        char buff[20];
        struct tm * timeinfo = localtime(time);
        strftime(buff, sizeof(buff), "%b %d %H:%M", timeinfo);

        printf("%s %s\n",str, buff);
        free(timeinfo);
}

int main(int argc, char *argv[])
{

        // Ensure correct argument count.

        if (argc > 2)
        {
                printf("Usage: SCAn [dirname]\n");
                return 1;
        }

        counter = malloc(2 * sizeof(int));
        *(counter) = 0;
        *(counter + 1) = 0;

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

        clock_t begin = clock();

        int error = traverse_directories(strlen(path), path);

        clock_t end = clock();
        double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
        if (error == 0)
        {
                printf("Traversed %d directories and found %d files in %fs\n", *counter, *(counter + 1), time_spent);
        }
        else
        {
                printf("Traveral failed in %fs\n", time_spent);
                return error;
        }
        return 0;
}
