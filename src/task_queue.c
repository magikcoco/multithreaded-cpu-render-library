#include "task_queue.h"
#include <stdio.h>
#include <stdlib.h>

/*
 * Queues up a task to be completed
 */
void queue_enqueue(TaskQueue* queue, void* (*function)(void*), void* arg) {
    // Create a task
    Task* task = malloc(sizeof(Task));

    // Check for failure
    if (task == NULL) {
        // TODO: Handle error
        perror("failed to allocate memory when enqueing a task");
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

void queue_enqueue_with_id(TaskQueueWithID* queue, void* (*function)(void*), void* arg, TaskID id){
    // Create a task
    TaskWithID* task = malloc(sizeof(Task));

    // Check for failure
    if (task == NULL) {
        // TODO: Handle error
        return;
    }

    // Initialize the task
    task->function = function;
    task->arg = arg;
    task->next = NULL;
    uuid_copy(*task->id, *id);

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

    pthread_cond_signal(&queue->not_empty);
    
    // Release the queue
    pthread_mutex_unlock(&queue->lock);
}

/*
 * Executes a single queued task, then destroys the task and advances the queue
 * Returns 0 if the queue is empty, or 1 if there is a task to complete
 */
int queue_dequeue(TaskQueue* queue, void* (**function)(void*), void** arg) {
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
 * Executes a single queued task, then destroys the task and advances the queue
 * Returns 0 if the queue is empty, or 1 if there is a task to complete
 */
void queue_dequeue_with_id(TaskQueueWithID* queue, void* (**function)(void*), void** arg, TaskID id) {
    // Acquire the queue lock
    pthread_mutex_lock(&queue->lock);

    // Wait while the queue is empty
    while (queue->head == NULL) {
        pthread_cond_wait(&queue->not_empty, &queue->lock);
    }

    // At this point, the queue is guaranteed to have at least one task
    TaskWithID* task = queue->head;
    *function = task->function;
    *arg = task->arg;

    if (!task->id) {
        perror("Failed to allocate memory for TaskID");
        free(task); // Ensure previously allocated memory is freed to avoid leaks
        return;
    }
    uuid_copy(*id, *task->id); //FIXME: id must be check and allocated if it doesnt exist

    // Advance the queue
    queue->head = task->next;
    if (queue->head == NULL) {
        queue->tail = NULL; // If the queue is now empty, update the tail as well
    }

    // Release the queue lock
    pthread_mutex_unlock(&queue->lock);

    // Free the allocated task, which is no longer needed
    free(task);
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
 * Initialize the id queue
 */
void queue_init_with_id(TaskQueueWithID* queue) {
    queue->head = NULL; // Empty queue
    queue->tail = NULL;
    pthread_mutex_init(&queue->lock, NULL); // Initialize the mutex
    pthread_cond_init(&queue->not_empty, NULL);
}

/*
 * Destroys the queue. No remaining tasks are executed.
 */
void queue_destroy(TaskQueue* queue) {
    void* (*function)(void*); // Function pointer
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