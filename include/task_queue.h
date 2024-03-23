#ifndef TASK_QUEUE_H
#define TASK_QUEUE_H

#include <pthread.h>
#include <uuid/uuid.h>

#define TaskID uuid_t*

typedef struct Task {
    void (*function)(void*);
    void* arg;
    struct Task* next;
} Task; // A task to be placed in the queue

typedef struct TaskWithID {
    TaskID id;
    void (*function)(void*);
    void* arg;
    struct TaskWithID* next;
} TaskWithID;

typedef struct {
    Task* head; // Pointer to the first task
    Task* tail; // Pointer to the last task
    pthread_mutex_t lock; // Mutex for thread safety
} TaskQueue; // A queue which contains tasks

typedef struct {
    TaskWithID* head; // Pointer to the first task
    TaskWithID* tail; // Pointer to the last task
    pthread_mutex_t lock; // Mutex for thread safety
    pthread_cond_t not_empty; // Condition variable to signal not empty
} TaskQueueWithID; // A queue which contains tasks

/*
 * Queues up a task to be completed
 */
void queue_enqueue(TaskQueue* queue, void (*function)(void*), void* arg);

/*
 * Queues up a task to be completed with a unique id included
 */
void queue_enqueue_with_id(TaskQueueWithID* queue, void (*function)(void*), void* arg, TaskID id);

/*
 * Modifies the given pointers to reflect the next task in the queue and then advances the queue
 * Returns 0 if the queue is empty, or 1 if there is a task to complete
 */
int queue_dequeue(TaskQueue* queue, void (**function)(void*), void** arg);

/*
 * Modifies the given pointers to reflect the next task in the queue and then advances the queue
 * If the queue is empty, it waits until there is work to do
 */
void queue_dequeue_with_id(TaskQueueWithID* queue, void (**function)(void*), void** arg, TaskID id);

/*
 * Initialize the queue
 */
void queue_init(TaskQueue* queue);

/*
 * Initialize the id queue
 */
void queue_init_with_id(TaskQueueWithID* queue);

/*
 * Destroys the queue. No remaining tasks are executed.
 */
void queue_destroy(TaskQueue* queue);

#endif