#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#include "sema.h"

sema_t sem_prio;

// Function to be run in the new thread
void *test_thread1(void *data)
{

    // Set the priority for this thread to the highest priority
    sema_set_priority(&sem_prio, 5);

    while (1)
    {
        sema_wait(&sem_prio);
        printf("Hello from test thread 1\n");
        usleep(1200000);
    }
}

void *test_thread2(void *data)
{

    // Set the priority for this thread to the second highest priority
    sema_set_priority(&sem_prio, 4);

    while (1)
    {
        sema_wait(&sem_prio);
        printf("Hello from test thread 2\n");
        usleep(1500000);
    }
}

void *test_thread3(void *data)
{

    // Set the priority for this thread to the third highest priority
    sema_set_priority(&sem_prio, 3);

    while (1)
    {
        sema_wait(&sem_prio);
        printf("Hello from test thread 3\n");
    }
}

void *thread_controller(void *data)
{

    while (1)
    {
        usleep(500000);

        // Notify highest waiting thread
        sema_post(&sem_prio);
    }
}

int main()
{
    pthread_t test_thread_id1;
    pthread_t test_thread_id2;
    pthread_t test_thread_id3;
    pthread_t thread_controller_id;

    // Initialize the priority semaphore
    sema_init(&sem_prio, 1, 0);

    // Create new threads
    if (pthread_create(&test_thread_id1, NULL, test_thread1, NULL))
    {
        fprintf(stderr, "Error creating thread\n");
        return 1;
    }
    if (pthread_create(&test_thread_id2, NULL, test_thread2, NULL))
    {
        fprintf(stderr, "Error creating thread\n");
        return 1;
    }
    if (pthread_create(&test_thread_id3, NULL, test_thread3, NULL))
    {
        fprintf(stderr, "Error creating thread\n");
        return 1;
    }

    // Create the control thread
    if (pthread_create(&thread_controller_id, NULL, thread_controller, NULL))
    {
        fprintf(stderr, "Error creating thread\n");
        return 1;
    }

    // Wait for the first thread to finish
    if (pthread_join(test_thread_id1, NULL))
    {
        fprintf(stderr, "Error joining thread\n");
        return 2;
    }

    printf("Exit test program \n");

    return 0;
}
