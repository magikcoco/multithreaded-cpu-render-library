#include "task_queue.h"
#include <stdio.h>
#include <stdlib.h>

/*
 * Queues up a task to be completed
 */
void queue_enqueue(TaskQueue* queue, void (*function)(void*), void* arg) {
    // Create a task
    Task* task = malloc(sizeof(Task));

    // Check for failure
    if (task == NULL) {
        // TODO: Handle error
        return;
    }

    // Initialize the task
    task->function = function;
    task->arg = arg;
    task->next = NULL;

    // Lock the queue through its mutex
    pthread_mutex_lock(&queue->lock);

    if (queue->tail == NULL) {
        // Queue is empty
        queue->head = task;
        queue->tail = task;
    } else {
        // Add to the end of the queue
        queue->tail->next = task;
        queue->tail = task;
    }
    
    // Release the queue
    pthread_mutex_unlock(&queue->lock);
}

/*
 * Executes a single queued task, then destroys the task and advances the queue
 * Returns 0 if the queue is empty, or 1 if there is a task to complete
 */
int queue_dequeue(TaskQueue* queue, void (**function)(void*), void** arg) {
    // Acquire the queue
    pthread_mutex_lock(&queue->lock);

    // Check if the queue is empty
    if (queue->head == NULL) {
        pthread_mutex_unlock(&queue->lock); // Release the queue if it is
        return 0; // Return failure in this case, since nothing was dequeued
    }

    // Adjust the function and arg parameters
    Task* task = queue->head; // Get the next task
    *function = task->function; // Set the function pointer
    *arg = task->arg; // Set the argument pointer

    // Advance the queue
    queue->head = task->next;
    if (queue->head == NULL) {
        queue->tail = NULL; // Queue became empty
    }

    // Release the queue
    pthread_mutex_unlock(&queue->lock);

    // Free the allocated task, which is no longer needed
    free(task);

    return 1; // Return success
}

/*
 * Initialize the queue
 */
void queue_init(TaskQueue* queue) {
    queue->head = NULL; // Empty queue
    queue->tail = NULL;
    pthread_mutex_init(&queue->lock, NULL); // Initialize the mutex
}

/*
 * Destroys the queue. No remaining tasks are executed.
 */
void queue_destroy(TaskQueue* queue) {
    void (*function)(void*); // Function pointer
    void* arg; // Argument pointer

    while (queue_dequeue(queue, &function, &arg)) { // Breaks loop when queue is empty
        // Do nothing with the functions
        if (arg != NULL) {
            free(arg); // Free the argument, if it exists
        }
    }

    // Finally destroy the mutex
    pthread_mutex_destroy(&queue->lock);
}