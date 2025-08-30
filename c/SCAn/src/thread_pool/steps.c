#include "steps.h"

steps_t *create_steps() {
    steps_t *steps = malloc(sizeof(steps_t));
    if (!steps) {
        return NULL;
    }
    steps->step_count = 0;
    steps->first = NULL;
    steps->last = NULL;
    steps->current = NULL;
    return steps;
}

step_t *add_step(steps_t *steps, char *step_name);

step_t *add_step(steps_t *steps, char *step_name) {
    step_t *new_step = malloc(sizeof(step_t));
    if (!new_step) {
        return NULL;
    }
    new_step->name = step_name;
    new_step->start_time = 0;
    new_step->end_time = 0;
    new_step->tasks = create_task_queue();
    new_step->next = NULL;

    if (!steps->last) {
        steps->first = new_step;
        steps->last = new_step;
    } else {
        steps->last->next = new_step;
        steps->last = new_step;
    }
    steps->step_count++;
    return new_step;
}

int add_task_to_step(step_t *step, int (*task_func)(task_queue_entry_arg_t *), task_queue_entry_arg_t *arg) {
    if (!step)
        return ILLEGAL_ARGS;
    return enqueue_task(step->tasks, task_func, arg);
}
