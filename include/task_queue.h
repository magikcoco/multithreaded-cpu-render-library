#ifndef TASK_QUEUE_H
#define TASK_QUEUE_H

#include <pthread.h>

typedef struct Task {
    void (*function)(void*);
    void* arg;
    struct Task* next;
} Task; // A task to be placed in the queue

typedef struct {
    Task* head;
    Task* tail;
    pthread_mutex_t lock;
} TaskQueue; // A queue which contains tasks

/*
 * Queues up a task to be completed
 */
void queue_enqueue(TaskQueue* queue, void (*function)(void*), void* arg);

/*
 * Executes a single queued task, then destroys the task and advances the queue
 * Returns 0 if the queue is empty, or 1 if there is a task to complete
 */
int queue_dequeue(TaskQueue* queue, void (**function)(void*), void** arg);

/*
 * Initialize the queue
 */
void queue_init(TaskQueue* queue);

/*
 * Destroys the queue. No remaining tasks are executed.
 */
void queue_destroy(TaskQueue* queue);

#endif