#ifndef STEPS_H
#define STEPS_H

#include "task_queue.h"

typedef struct step_t step_t;
typedef struct steps_t steps_t;

struct step_t {
    char *name;
    long start_time;
    long end_time;
    task_queue_t *tasks;
    step_t *next;
};

struct steps_t {
    int step_count;
    step_t *first;
    step_t *last;
    step_t *current;
};

step_t *add_step(steps_t *steps, char *step_name);

#endif