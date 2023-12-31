#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>

// Signal data sent via shared memory
struct SignalData
{
    int a;
    int b;
};

struct SignalData *shared_data;

void signal_handler(int signo, siginfo_t *info, void *context)
{
    // Use shared_data directly, not from info->si_value.sival_ptr

    if (signo == SIGUSR1)
    {
        printf("Received SIGUSR1\n");
        printf("si_code: %d, si_value: %d\n", info->si_code, info->si_value.sival_int);
    }

    printf("Data: a=%d, b=%d\n", shared_data->a, shared_data->b);

    // Clean up shared memory
    if (shm_unlink("/signal_shm") == -1)
    {
        perror("Failed to unlink shared memory");
        return;
    }
}

int main()
{
    //Open shared memory to be used as part of signals data
    int fd = shm_open("/signal_shm", O_CREAT | O_RDWR, 0666);
    if (fd == -1)
    {
        perror("shm_open failed");
        return 1;
    }

    ftruncate(fd, sizeof(struct SignalData));
    shared_data = mmap(NULL, sizeof(struct SignalData), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shared_data == MAP_FAILED)
    {
        perror("mmap failed");
        return 1;
    }

    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = signal_handler;

    if (sigaction(SIGUSR1, &sa, NULL) == -1)
    {
        perror("sigaction");
        return 1;
    }

    printf("My PID is: %d\n", getpid());

    while (1)
    {
        sleep(1);
    }

    return 0;
}
