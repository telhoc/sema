#include <stdio.h>
#include <pthread.h>

// Function to be run in the new thread
void* test_thread(void* data) {
    printf("Hello from test thread \n");
    return NULL;
}

int main() {
    pthread_t test_thread_id;

    // Create a new thread
    if (pthread_create(&test_thread_id, NULL, test_thread, NULL)) {
        fprintf(stderr, "Error creating thread\n");
        return 1;
    }

    // Wait for the thread to finish
    if (pthread_join(test_thread_id, NULL)) {
        fprintf(stderr, "Error joining thread\n");
        return 2;
    }

    printf("Exit test program \n");

    return 0;
}
