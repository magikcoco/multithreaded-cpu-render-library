#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "task_queue.h"

#define MAX_TASKS 10

// Task structure
typedef struct PoolTask {
    void (*function)(void* arg);
    void *arg;
} PoolTask;

// Batch control structure
typedef struct PoolTaskBatch {
    int batch_id;
    int remaining_tasks;
    pthread_cond_t batch_complete;
    PoolTask* tasks;
} PoolTaskBatch;

// Thread pool structure
typedef struct thread_pool {
    TaskQueue task_queue;
    pthread_mutex_t lock;
    pthread_cond_t notify;
    pthread_t* worker_threads;
    PoolTaskBatch* batches;
} ThreadPool;

int sys_core_count;
atomic_bool shutdown_flag = ATOMIC_VAR_INIT(false); // When true triggers shutdown of application

/*
 * Calculates and returns the number of cores available on the system
 * If it fails to determine this, it defaults to a value of 4
 */
int calc_core_count() {
    // Get the number of CPU cores
    int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    if (num_cores < 1) {
        perror("Couldn't detect available cores, defaulting to 4 threads\n");
        return 4; // Default if unable to determine
    }
    printf("cpu cores available: %d\n", num_cores);
    return num_cores;
}

void* worker_function(void *arg) {
    ThreadPool *pool = (ThreadPool*) arg;
    void (*task_function)(void*);
    void* task_arg;

    while (!atomic_load(&shutdown_flag)) {
        // The next task should be dequeued
        queue_dequeue(&pool->task_queue, &task_function, &task_arg); // This function will sleep until something enters the queue
    }

    return NULL;
}

void initialize_thread_pool(ThreadPool* pool) {
    // System core count is the number of threads and the starting maximum for batched worker thread requets
    // The main thread alreadys exists so -1 to the return value
    sys_core_count = (calc_core_count() - 1);

    // Allocate memory for workers
    pool->worker_threads = malloc(sys_core_count * sizeof(pthread_t));
    if (pool->worker_threads == NULL) {
        perror("Failed to allocate memory for worker threads\n");
        exit(1);
    }

    // Allocate memory for batches
    pool->batches = malloc(sys_core_count * sizeof(PoolTaskBatch)); // This is likely to be resized, whatever it ends up being
    if (pool->batches == NULL) {
        perror("Failed to allocate memory for batched tasks\n");
        free(pool->worker_threads); // Clean up previously allocated memory
        exit(1);
    }

    // Initialize workers
    for (int i = 0; i < sys_core_count; i++) {
        if (pthread_create(&pool->worker_threads[i], NULL, worker_function, (void*)pool) != 0) {
            perror("Failed to create worker thread");
            exit(0); //TODO: handle this better
        }
    }
}