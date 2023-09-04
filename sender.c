#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

struct SignalData
{
    int a;
    int b;
};

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <PID>\n", argv[0]);
        return 1;
    }

    int fd = shm_open("/signal_shm", O_RDWR, 0666);
    if (fd == -1)
    {
        perror("shm_open failed");
        return 1;
    }

    ftruncate(fd, sizeof(struct SignalData));
    struct SignalData *shared_data = mmap(NULL, sizeof(struct SignalData), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shared_data == MAP_FAILED)
    {
        perror("mmap failed");
        return 1;
    }

    // Set the values to send
    shared_data->a = 5;
    shared_data->b = 10;

    union sigval value;
    value.sival_int = 42; // Some integer value to send

    int pid = atoi(argv[1]);
    if (sigqueue(pid, SIGUSR1, value) == -1)
    {
        perror("sigqueue");
        return 2;
    }

    printf("Signal sent.\n");

    return 0;
}
