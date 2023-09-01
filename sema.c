#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wchar.h>
#include <time.h>
#include <assert.h>
#include <sys/socket.h>

#include "sema.h"

#define noSEMA_DEBUG

void sema_printf(const char *format, ...)
{
#ifdef SEMA_DEBUG
  char buffer[256];

  va_list args;
  va_start(args, format);
  vsprintf(buffer, format, args);
  printf("%s", buffer);
  va_end(args);
#endif
}

/**
 * @brief Print all threads for all priorities and their status
 *
 * @param sema
 */
void SemaPrintThreadIds(sema_t *sema)
{
#ifdef SEMA_DEBUG
  int i, j;
  char pstring[1024];
  char *p;

  memset(pstring, 0, sizeof(pstring));
  p = pstring;

  for (i = 0; i < SEMA_NUM_PRIORITIES; i++)
  {
    p += sprintf(p, "%d: ", i);
    for (j = 0; j < SEMA_NUM_PER_PRIORITY; j++)
    {
      p += sprintf(p, "%lu(%d) ", sema->waiting_thread[i][j], sema->waiting_thread_status[i][j]);
    }
    p += sprintf(p, "\n");
  }
  printf("%s", pstring);
#endif
}

/**
 * @brief Initalize a semaphore
 *
 * @param sema The semaphore to initalize
 * @param pshared The pthread pshared value
 * @param value The semaphore value
 */
void sema_init(sema_t *sema, int pshared, unsigned int value)
{
  int i, j;

  for (i = 0; i < SEMA_NUM_PRIORITIES; i++)
  {
    sema->priority_rr_pos[i] = 0;
  }

  for (i = 0; i < SEMA_NUM_PRIORITIES; i++)
  {
    for (j = 0; j < SEMA_NUM_PER_PRIORITY; j++)
    {
      sem_init(&sema->priority_sem[i][j], pshared, 0);
      sema->waiting_thread_status[i][j] = 0;
    }
  }
  sema->pshared = pshared;
  sema->semval = value;
  sema->numWaiting = 0;
  pthread_mutex_init(&sema->semLock, NULL);
  pthread_mutex_unlock(&sema->semLock);
}

/**
 * @brief Get the priority and position of the calling thread
 *
 * @param sema The priority semaphore
 * @param priority the returned priority
 * @param position the position of the thread in this priority
 * @return int
 */
int SemaGetPriority(sema_t *sema, int *priority, int *position)
{
  // Iterate through the used threads and find the calling thread
  int i, j;
  for (i = 0; i < SEMA_NUM_PRIORITIES; i++)
  {
    for (j = 0; j < SEMA_NUM_PER_PRIORITY; j++)
    {
      // Only check initalized (set) threads
      if (sema->waiting_thread_status[i][j])
      {
        // Did we find the calling thread?
        if (pthread_equal(pthread_self(), sema->waiting_thread[i][j]))
        {
          *priority = i;
          *position = j;
          sema_printf("Found prio %d pos %d thread %lu %lu cmp %d\n",
                      i, j, pthread_self(), sema->waiting_thread[i][j],
                      pthread_equal(pthread_self(), sema->waiting_thread[i][j]));
          return (sema->waiting_thread_status[i][j]);
        }
      }
    }
  }
  return -1;
}

/**
 * @brief Internal function the sets the priority and status of a thread
 *
 * @param sema
 * @param waitThread
 * @param priority
 * @param status_to_set
 * @return int
 */
int SemaSetPriorityInternal(sema_t *sema, pthread_t waitThread, int priority,
                            int status_to_set)
{
  int j;

  for (j = 0; j < SEMA_NUM_PER_PRIORITY; j++)
  {

    // Check if priority is set for a dead thread, clear it
    if (sema->waiting_thread_status[priority][j] == SEMA_STATUS_PRIOITY_SET)
    {
      if (pthread_kill(sema->waiting_thread[priority][j], 0) == ESRCH)
      {
        sema_printf("Old thread %lu is dead, reset\n", sema->waiting_thread[priority][j]);
        sema->waiting_thread_status[priority][j] = 0;
        memset(&sema->waiting_thread[priority][j], 0, sizeof(pthread_t));
      }
    }

    if (!sema->waiting_thread_status[priority][j])
    {
      sema->waiting_thread_status[priority][j] = status_to_set;
      memcpy(&sema->waiting_thread[priority][j], &waitThread, sizeof(pthread_t));
      sema_printf("SetPriority %d pos %d for id %lu (check %lu)\n", priority, j,
                  waitThread, sema->waiting_thread[priority][j]);
      SemaPrintThreadIds(sema);

      pthread_mutex_unlock(&sema->semLock);
      return j;
    }
  }

  pthread_mutex_unlock(&sema->semLock);
  return -1;
}

/**
 * @brief Set the priority of the calling thread
 *
 * @param sema The priority semaphore
 * @param priority The priority to set
 * @return int
 */
int sema_set_priority(sema_t *sema, int priority)
{
  int priority_a;
  int position_a;
  int status_to_set;

  status_to_set = SEMA_STATUS_PRIOITY_SET;

  pthread_mutex_lock(&sema->semLock);
  if (SemaGetPriority(sema, &priority_a, &position_a) < 0)
  {
    return SemaSetPriorityInternal(sema, pthread_self(), priority, status_to_set);
  }
  else
  {
    if (priority_a == priority)
    {
      sema_printf("SetPriority unchanged at prio %d pos %d \n",
                  priority_a, position_a);

      pthread_mutex_unlock(&sema->semLock);
      return position_a;
    }
    else
    {
      // keep the old status, just change priority, after resetting
      printf("SetPriority: change %d-->%d\n", priority_a, priority);
      status_to_set = sema->waiting_thread_status[priority_a][position_a];
      sema->waiting_thread_status[priority_a][position_a] = 0;
      memset(&sema->waiting_thread[priority_a][position_a], 0, sizeof(pthread_t));
      return SemaSetPriorityInternal(sema, pthread_self(), priority, status_to_set);
    }
  }
}

/**
 * @brief Find the priority and position of the highest waiting thread
 *
 * @param sema The priority semaphore
 * @param priority
 * @param position
 * @return int
 */
int SemaGetPost(sema_t *sema, int *priority, int *position)
{
  int i, j;
  int rr_pos;

  for (i = (SEMA_NUM_PRIORITIES - 1); i >= 0; i--)
  {
    rr_pos = sema->priority_rr_pos[i];
    for (j = 0; j < SEMA_NUM_PER_PRIORITY; j++)
    {
      int rr_pos_use = ((j + rr_pos) % SEMA_NUM_PER_PRIORITY);
      if (sema->waiting_thread_status[i][rr_pos_use] == SEMA_STATUS_PRIOITY_WAIT)
      {
        *priority = i;
        *position = rr_pos_use;
        sema->priority_rr_pos[i] = ((rr_pos_use + 1) % SEMA_NUM_PER_PRIORITY);
        return (sema->waiting_thread_status[i][rr_pos_use]);
      }
    }
  }
  return -1;
}

/**
 * @brief Set the status of a specific priority and position to wait
 *
 * @param sema
 * @param priority
 * @param position
 * @return int
 */
int SemaSetWait(sema_t *sema, int priority, int position)
{

  if (sema->waiting_thread_status[priority][position] == SEMA_STATUS_PRIOITY_SET)
  {
    sema->waiting_thread_status[priority][position] = SEMA_STATUS_PRIOITY_WAIT;
    return 1;
  }
  return -1;
}

/**
 * @brief Unset the status of a specific priority and position to wait
 *
 *
 * @param sema
 * @param priority
 * @param position
 * @return int
 */
int SemaUnsetWait(sema_t *sema, int priority, int position)
{

  if (sema->waiting_thread_status[priority][position] == SEMA_STATUS_PRIOITY_WAIT)
  {
    sema->waiting_thread_status[priority][position] = SEMA_STATUS_PRIOITY_SET;
    return 1;
  }
  return -1;
}

/**
 * @brief Perform a wait on the priority semaphore
 *
 * @param sema
 */
void sema_wait(sema_t *sema)
{
  int priority;
  int position;

  pthread_mutex_lock(&sema->semLock);
  if (SemaGetPriority(sema, &priority, &position) < 0)
  {
    priority = 0;
    position = SemaSetPriorityInternal(sema, pthread_self(), priority,
                                       SEMA_STATUS_PRIOITY_SET);
  }
  SemaSetWait(sema, priority, position);

  if (sema->semval > 0)
  {
    sema->semval--;
    SemaUnsetWait(sema, priority, position);

    sema_printf("sema_wait: semval %d proceed. Numwaiting %d. Status %d\n",
                sema->semval + 1, sema->numWaiting, sema->waiting_thread_status[priority][position]);
    SemaPrintThreadIds(sema);
#
    pthread_mutex_unlock(&sema->semLock);
    return;
  }
  else
  {
    sema->numWaiting++;

    sema_printf("sema_wait: wait semval %d prio %d pos %d numwaiting %d %lu stat %d\n",
                sema->semval, priority, position,
                sema->numWaiting, pthread_self(),
                sema->waiting_thread_status[priority][position]);
    SemaPrintThreadIds(sema);

    sem_init(&sema->priority_sem[priority][position], sema->pshared, 0);
    pthread_mutex_unlock(&sema->semLock);
    sem_wait(&sema->priority_sem[priority][position]);
    sema->numWaiting--;
    sema->semval--;

    sema_printf("sema_wait: got wait semval %d prio %d pos %d numwaiting %d %lu stat %d\n",
                sema->semval, priority, position,
                sema->numWaiting, pthread_self(),
                sema->waiting_thread_status[priority][position]);

    sema->waiting_thread_status[priority][position] = 0;
    memset(&sema->waiting_thread[priority][position], 0, sizeof(pthread_t));
  }
  return;
}

/**
 * @brief Perform a post to the priority semaphore
 *
 * @param sema
 */
void sema_post(sema_t *sema)
{
  int priority;
  int position;

  pthread_mutex_lock(&sema->semLock);
  if (SemaGetPost(sema, &priority, &position) < 0)
  {
    // Error? No waiting threads found
    sema->semval++;

    sema_printf("sema_post: No waiting threads found. Semval %d. nm %d\n",
                sema->semval, sema->numWaiting);
    SemaPrintThreadIds(sema);

    pthread_mutex_unlock(&sema->semLock);
    return;
  }
  else
  {
    sema_printf("sema_post: Found waiting thread prio %d pos %d ", priority, position);
  }

  sema->semval++;
  SemaUnsetWait(sema, priority, position);

  SemaPrintThreadIds(sema);
  sema_printf("sema_post: prepost semval %d numwaiting %d\n", sema->semval, sema->numWaiting);

  sem_post(&sema->priority_sem[priority][position]);
  pthread_mutex_unlock(&sema->semLock);
}

int sema_getvalue(sema_t *sema, int *semval)
{
  *semval = sema->semval;
  return 0;
}
