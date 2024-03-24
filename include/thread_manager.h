#ifndef THREAD_POOLING_H
#define THREAD_POOLING_H

#include "task_queue.h"

// Task structure
typedef struct PoolTask {
    void (*function)(void* arg);
    void *arg;
} PoolTask;

TaskID submit_task(PoolTask* task);
void wait_for_task_completion(TaskID id);

#endif // THREAD_POOLING_H