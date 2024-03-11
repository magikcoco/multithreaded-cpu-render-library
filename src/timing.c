#include <time.h>
#include "timing.h"

// Frame time, thread local to prevent concurrency issues
_Thread_local struct timespec last_frame_time = { .tv_sec = -1, .tv_nsec = -1 };

/*
 * initializes timespec struct with the current time
 */
void get_current_time(struct timespec *time) {
    clock_gettime(CLOCK_MONOTONIC, time);
}

/* 
 * calculates the time difference in nanoseconds
 */
long time_diff_nanoseconds(struct timespec *start, struct timespec *end) {
    return (end->tv_sec - start->tv_sec) * 1000000000L + (end->tv_nsec - start->tv_nsec);
}

/*
 * Function to delay execution to target a specific frame rate
 */
void frame_rate_control(int targetFPS) {
    if (targetFPS <= 0) return; // Prevent division by zero

    if(last_frame_time.tv_sec == -1 || last_frame_time.tv_nsec == -1) get_current_time(&last_frame_time);

    struct timespec currentTime, sleepTime;
    long targetFrameDuration = 1000000000L / targetFPS; // Nanoseconds per frame
    get_current_time(&currentTime);

    long timeTaken = time_diff_nanoseconds(&last_frame_time, &currentTime);
    long delayTime = targetFrameDuration - timeTaken;

    if (delayTime > 0) {
        sleepTime.tv_sec = delayTime / 1000000000L;
        sleepTime.tv_nsec = delayTime % 1000000000L;
        nanosleep(&sleepTime, NULL);
    }

    // Update the last frame time to now, after the delay
    get_current_time(&last_frame_time);
}