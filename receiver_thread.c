#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <pthread.h>

struct TwoIntegers
{
    int a;
    int b;
};

struct TwoIntegers *shared_data;

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

int main()
{
    int fd = shm_open("/my_shm", O_CREAT | O_RDWR, 0666);
    if (fd == -1)
    {
        perror("shm_open failed");
        return 1;
    }

    ftruncate(fd, sizeof(struct TwoIntegers));
    shared_data = mmap(NULL, sizeof(struct TwoIntegers), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
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

    printf("My PID is: %d\n", getpid());

    pthread_join(sig_thread_id, NULL);

    return 0;
}
