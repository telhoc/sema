#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <pthread.h>

// Signal data sent via shared memory
struct SignalData
{
    int a;
    int b;
};

struct SignalData *shared_data;

/**
 * @brief Main signal threead that receives signal data
 *
 * @param arg
 * @return void*
 */
void *signal_thread(void *arg)
{
    sigset_t set;
    int signo;

    // Initialize the signal set and add SIGUSR1
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);

    // Wait for the signal
    while (1)
    {
        sigwait(&set, &signo);
        printf("Received signal %d\n", signo);

        if (signo == SIGUSR1)
        {
            printf("Received SIGUSR1\n");

            printf("Data: a=%d, b=%d\n", shared_data->a, shared_data->b);
        }
    }

    return NULL;
}

/**
 * @brief Example work thread
 *
 * @param arg
 * @return void*
 */
void *work_thread(void *arg)
{
    while (1)
    {
        printf("Working...\n");
        sleep(2);
    }

    return NULL;
}

int main()
{
    // Open shared memory to be used as part of signals data
    int fd = shm_open("/signal_shm", O_CREAT | O_RDWR, 0666);
    if (fd == -1)
    {
        perror("shm_open failed");
        return 1;
    }

    // Configure the shared memory to map to the signal data struct
    ftruncate(fd, sizeof(struct SignalData));
    shared_data = mmap(NULL, sizeof(struct SignalData), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shared_data == MAP_FAILED)
    {
        perror("mmap failed");
        return 1;
    }

    pthread_t sig_thread_id, work_thread_id;

    // Block SIGUSR1 in all threads by default
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    pthread_sigmask(SIG_BLOCK, &set, NULL);

    // Create the signal-handling thread
    pthread_create(&sig_thread_id, NULL, signal_thread, NULL);

    // Create an example "working" thread
    pthread_create(&work_thread_id, NULL, work_thread, NULL);

    printf("My PID is: %d\n", getpid());

    pthread_join(sig_thread_id, NULL);
    pthread_join(work_thread_id, NULL);

    return 0;
}
