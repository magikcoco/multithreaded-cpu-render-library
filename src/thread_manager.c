#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <uuid/uuid.h>
#include "thread_manager.h"
#include "uthash.h"
#include "function_mapping.h"

/*
 * Structure for holding UUIDs and condition variables in a hashmap
 */
typedef struct CondIDPair {
    TaskID task_id; // Pointer to the uuid
    pthread_cond_t task_complete; // Condition variable
    pthread_mutex_t mutex; // Mutex for the condition variable
    UT_hash_handle hh; // For hashmap
} CondIDPair;

/*
 * Thread Pool structure
 */
typedef struct ThreadPool {
    TaskQueueWithID task_queue; // Task queue
    pthread_mutex_t lock; // For locking the thread pool
    pthread_t* worker_threads; // The worker threads in the pool
} ThreadPool;

atomic_bool shutdown_flag = ATOMIC_VAR_INIT(false); // When true triggers shutdown of application
ThreadPool* thread_pool = NULL; // The thread pool for cpu intensive tasks
pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex for the initialization of the cpu thread pool
atomic_bool init_flag = ATOMIC_VAR_INIT(true); // Flag for the initialization of the cpu thread pool
CondIDPair* hashmap = NULL; // Global hashmap for storing CondIDPair structures across both thread pools
pthread_mutex_t hashmap_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex for locking access to the hashmap
_Thread_local int worker_thread_number = 0; // Used to id threads, for logging purposes
atomic_int thread_counter = ATOMIC_VAR_INIT(1); // Counts up for each thread
int sub_task_counter = 0; // Easier to use a mutex than an atomic int here
pthread_mutex_t sub_task_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex for locking the sub task counter
int max_sub_tasks;

/*
 * Calculates and returns the number of cores available on the system
 * If it fails to determine this, it defaults to a value of 4
 */
int calc_core_count() {
    // Get the number of CPU cores
    int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    if (num_cores < 1) {
        perror("Couldn't detect available cores, defaulting to 4 threads\n");
        max_sub_tasks = 1;
        return 4; // Default if unable to determine
    } else if (num_cores < 4 && num_cores > 1) {
        max_sub_tasks = 1;
    } else if (num_cores == 1) {
        max_sub_tasks = 0; // Must be zero or there will be deadlock later
    } else {
        max_sub_tasks = num_cores/4;
    }
    printf("cpu cores available: %d\n", num_cores);
    return num_cores;
}

/*
 * Find a condition variable by the uuid, and remove it from the hashmap after retreival
 */
CondIDPair* find_cond_id_pair(TaskID task_id) {
    printf("Worker thread %d: searching for condition/id pair\n", worker_thread_number);
    CondIDPair* pair = NULL;
    pthread_mutex_lock(&hashmap_mutex); // Lock the hashmap
    HASH_FIND_PTR(hashmap, task_id, pair); // Retrieve based on TaskID
    pthread_mutex_unlock(&hashmap_mutex);
    printf("Worker thread %d: found condition/id pair\n", worker_thread_number);
    return pair; // NULL if not found
}

void remove_cond_id_pair(CondIDPair* pair){
    pthread_mutex_lock(&hashmap_mutex); // Lock the hashmap
    HASH_DEL(hashmap, pair);
    pthread_mutex_unlock(&hashmap_mutex);
}

/*
 * Completely deallocate a CondIDPair
 */
void remove_and_destroy_cond_id_pair(CondIDPair** pair_ptr) {
    if(!pair_ptr || !*pair_ptr){
        return; // Check if the pointer exists
    }

    CondIDPair* pair = *pair_ptr; // Get the structure from the pointer to the pointer

    pthread_cond_destroy(&pair->task_complete); // Destroy the condition variable and mutex
    pthread_mutex_destroy(&pair->mutex);

    memset(&pair->task_id, 0, sizeof(uuid_t)); // Zeroes out the UUID

    free(pair); // Free the pointer itself
    *pair_ptr = NULL; // Dangling pointer
}

/*
 * Worker function waits for a task to be enqueued and then signals that task is complete
 */
void* worker_function(void *arg) {
    ThreadPool *pool = (ThreadPool*) arg;
    void* (*task_function)(void*); // Function pointer modified by dequeue function
    void* task_arg; // Argument pointer modified by queue function
    TaskID id;

    // For logging purposes, set up thread IDs
    worker_thread_number = atomic_fetch_add(&thread_counter, 1);

    printf("Worker thread %d: started\n", worker_thread_number);

    while (!atomic_load(&shutdown_flag)) {
        printf("Worker thread %d: dequeueing task\n", worker_thread_number);

        // This function will sleep until something enters the queue
        queue_dequeue_with_id(&pool->task_queue, &task_function, &task_arg, &id); // Gets the task and the id

        //printf("Worker thread %d: received \"%s\"\n", worker_thread_number, get_function_name(task_function));
        printf("Worker thread %d: received a task\n", worker_thread_number); //TODO: make the above work instead

        task_function(task_arg); // Execute the task

        printf("Worker thread %d: finished task\n", worker_thread_number);

        CondIDPair* pair = find_cond_id_pair(id);
        if(pair){
            printf("Worker thread %d: processing pair\n", worker_thread_number);
            pthread_mutex_lock(&pair->mutex);
            pthread_cond_signal(&pair->task_complete);
            pthread_mutex_unlock(&pair->mutex);
            remove_cond_id_pair(pair);
            remove_and_destroy_cond_id_pair(&pair);
            printf("Worker thread %d: signaled task complete\n", worker_thread_number);
        } else { // The pair is null so the wait thread should still see the task completed in this case
            printf("Worker thread %d: failed to find uuid for finished task in hashmap\n", worker_thread_number);
        }
    }

    printf("Worker thread %d: closing\n", worker_thread_number);

    return NULL;
}

/*
 * Initializes the cpu thread pool
 */
void initialize_thread_pool() {
    // If the CPU thread pool already exists, don't reinitialize it
    if(thread_pool || !atomic_load(&init_flag)) {
        perror("initialization of already initialized thread pool");
        return;
    }

    // Flip the flag
    atomic_store(&init_flag, false);

    // Get the core count
    int cores = calc_core_count();

    // Allocate memory for the thread pool itself
    thread_pool = malloc(sizeof(ThreadPool));
    if(!thread_pool){
        perror("failed to allocate memory for cpu thread pool");
        exit(1);
    }

    // Allocate task queue
    queue_init_with_id(&thread_pool->task_queue);

    // Initialize other values
    pthread_mutex_init(&thread_pool->lock, NULL);

    // Allocate memory for workers
    thread_pool->worker_threads = malloc(cores * sizeof(pthread_t));
    if (!thread_pool->worker_threads) {
        perror("failed to allocate memory for worker threads\n");
        free(thread_pool);
        exit(1);
    }

    // Initialize workers
    for (int i = 0; i < cores; i++) {
        int result = pthread_create(&thread_pool->worker_threads[i], NULL, worker_function, (void*)thread_pool);
        if (result) { // result is 0 on success
            perror("failed to create worker thread in cpu worker pool");
            free(thread_pool->worker_threads);
            free(thread_pool);
            exit(1);
        }
    }
}

/*
 * Create a conditional variable to pair with the uuid and add it to the hashmap
 */
void add_id_cond_pair(TaskID id){
    CondIDPair* pair = malloc(sizeof(CondIDPair)); // Dynamically allocate the memory needed
    if(!pair){ // Check for failure
        perror("failed to allocate for new condition/id pair");
        return;
    }

    // Copy the uuid into the structure
    uuid_copy(pair->task_id, id);

    // Initial condition variable and mutex
    pthread_cond_init(&pair->task_complete, NULL);
    pthread_mutex_init(&pair->mutex, NULL);

    // Add the struct to the hashmap
    pthread_mutex_lock(&hashmap_mutex);
    HASH_ADD_PTR(hashmap, task_id, pair);
    pthread_mutex_unlock(&hashmap_mutex);
}

/*
 * Add a task to the cpu thread pool
 */
TaskID* submit_task(PoolTask* task) {
    // Lazy initialization
    if(atomic_load(&init_flag)){
        printf("here\n");
        pthread_mutex_lock(&init_mutex);
        if(!thread_pool){
            initialize_thread_pool();
        }
        pthread_mutex_unlock(&init_mutex);
    }

    // Lock the thread pool
    pthread_mutex_lock(&thread_pool->lock);

    TaskID* id = malloc(sizeof(uuid_t));
    if(!id){
        perror("allocating id failed");
        return NULL;
    }
    uuid_generate(*id); // uuid_t*

    add_id_cond_pair(*id);

    // Create a special id to identify the task later
    queue_enqueue_with_id(&thread_pool->task_queue, task->function, task->arg, *id);

    // Unlock the thread pool
    pthread_mutex_unlock(&thread_pool->lock);

    return id;
}

/*
 * Add a task to the cpu thread pool which submits subtasks. Use this to prevent deadlocking.
 * If there are too many subtasks spawning tasks, the task ID will return null and no task will be submitted.
 */
TaskID* submit_task_with_subtask(PoolTask* task) {
    // Check if cap is reached
    pthread_mutex_lock(&sub_task_mutex);
    if(sub_task_counter < max_sub_tasks){
        sub_task_counter++;
        pthread_mutex_unlock(&sub_task_mutex);
    } else {
        pthread_mutex_unlock(&sub_task_mutex);
        return NULL;
    }

    // Lazy initialization
    if(atomic_load(&init_flag)){
        pthread_mutex_lock(&init_mutex);
        if(!thread_pool){
            initialize_thread_pool();
        }
        pthread_mutex_unlock(&init_mutex);
    }

    // Lock the thread pool
    pthread_mutex_lock(&thread_pool->lock);

    TaskID* id = NULL;
    uuid_generate(*id); // uuid_t*

    add_id_cond_pair(*id);

    // Create a special id to identify the task later
    queue_enqueue_with_id(&thread_pool->task_queue, task->function, task->arg, *id);

    // Unlock the thread pool
    pthread_mutex_unlock(&thread_pool->lock);

    return id;
}

/*
 * Waits for a condition signal if the given TaskID doesnt come up null in the hashmap
 */
void wait_for_task_completion(TaskID id){
    if(!id) return; // If its null it's faster to return now
    CondIDPair* pair;
    pthread_mutex_lock(&hashmap_mutex);
    HASH_FIND_PTR(hashmap, &id, pair); // Returns null if not found
    pthread_mutex_unlock(&hashmap_mutex);
    while(!pair) {
        pthread_cond_wait(&pair->task_complete, &pair->mutex);
    }
}

/*
 * Waits for a condition signal if the given TaskID doesnt come up null in the hashmap
 * Paired with subtask spawning tasks
 */
void wait_for_task_with_subtask_completion(TaskID id){
    if(!id) return; // If its null it's faster to return now
    CondIDPair* pair;
    pthread_mutex_lock(&hashmap_mutex);
    HASH_FIND_PTR(hashmap, &id, pair); // Returns null if not found
    pthread_mutex_unlock(&hashmap_mutex);
    while(!pair) {
        pthread_cond_wait(&pair->task_complete, &pair->mutex);
    }
    pthread_mutex_lock(&sub_task_mutex);
    sub_task_counter--; // This part is why this function exists
    pthread_mutex_unlock(&sub_task_mutex);
}

/*
 * Flips the shutdown flag to true
 */
void signal_shutdown(){
    printf("Worker thread %d: signal shutdown of application\n", worker_thread_number);
    atomic_store(&shutdown_flag, true);
}

/*
 * Retreives the current state of the shutdown flag
 */
bool is_application_shutdown(){
    return atomic_load(&shutdown_flag);
}