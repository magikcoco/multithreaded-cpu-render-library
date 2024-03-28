#ifndef THREAD_POOLING_H
#define THREAD_POOLING_H

#include "task_queue.h"
#include <stdbool.h>

// Task structure
typedef struct PoolTask {
    void (*function)(void* arg);
    void *arg;
} PoolTask;

/*
 * Submit a task to the thread pool for completion
 */
TaskID submit_task(PoolTask* task);

/*
 * Wait for the task identified by the given ID to complete
 */
void wait_for_task_completion(TaskID id);

/*
 * Trigger application shutdown. Worker threads will all stop wworking and return.
 */
void signal_shutdown();

/*
 * True if application shutdown has triggered, false otherwise
 */
bool is_application_shutdown();

#endif // THREAD_POOLING_H