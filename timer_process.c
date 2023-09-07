#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdlib.h>

// Signal data sent via shared memory
struct SignalData
{
    int signal_type;
    int signal_value;
};

struct SignalData *shared_data;

// Struct for holding our timers
struct TimerInfo
{
    timer_t timer;
    int id;
    struct itimerspec its;
    struct sigevent sev;
};

// Define two timers
struct TimerInfo timerInfo1 = {.id = 1};
struct TimerInfo timerInfo2 = {.id = 2};

void timer_handler(int signo, siginfo_t *info, void *context)
{
    struct TimerInfo *timerInfo = (struct TimerInfo *)info->si_value.sival_ptr;
    if (timerInfo->id == 1)
    {
        printf("We got timer 1 \n");
    }
    else if (timerInfo->id == 2)
    {
        printf("We got timer 2 \n");
    }
}

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

            printf("Data: a=%d, b=%d\n", shared_data->signal_type, shared_data->signal_value);
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

void setup_timers()
{

    // Define the signal to be used for the timers
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = timer_handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGUSR2, &sa, NULL) == -1)
    {
        perror("sigaction for timer");
        exit(1);
    }

    // Define and create the 1st timer
    timerInfo1.sev.sigev_notify = SIGEV_SIGNAL;
    timerInfo1.sev.sigev_signo = SIGUSR2;
    timerInfo1.sev.sigev_value.sival_ptr = &timerInfo1;
    if (timer_create(CLOCK_REALTIME, &timerInfo1.sev, &timerInfo1.timer) == -1)
    {
        perror("timer1 create error");
        exit(1);
    }

    // Define and create the 2nd timer
    timerInfo2.sev.sigev_notify = SIGEV_SIGNAL;
    timerInfo2.sev.sigev_signo = SIGUSR2;
    timerInfo2.sev.sigev_value.sival_ptr = &timerInfo2;
    if (timer_create(CLOCK_REALTIME, &timerInfo2.sev, &timerInfo2.timer) == -1)
    {
        perror("timer2 create error");
        exit(1);
    }

    // Start timer 1
    timerInfo1.its.it_value.tv_sec = 3; // 3 seconds
    timerInfo1.its.it_value.tv_nsec = 0;
    timerInfo1.its.it_interval.tv_sec = 0;
    timerInfo1.its.it_interval.tv_nsec = 0;

    if (timer_settime(timerInfo1.timer, 0, &timerInfo1.its, NULL) == -1)
    {
        perror("timer_settime");
        exit(1);
    }

    // Start timer 2
    timerInfo2.its.it_value.tv_sec = 5; // 5 seconds
    timerInfo2.its.it_value.tv_nsec = 0;
    timerInfo2.its.it_interval.tv_sec = 0;
    timerInfo2.its.it_interval.tv_nsec = 0;

    if (timer_settime(timerInfo2.timer, 0, &timerInfo2.its, NULL) == -1)
    {
        perror("timer_settime");
        exit(1);
    }
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

    // Setup the timers
    setup_timers();

    pthread_join(sig_thread_id, NULL);
    pthread_join(work_thread_id, NULL);

    return 0;
}
