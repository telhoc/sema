#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <wchar.h>
#include <time.h>
#include <assert.h>
#include <sys/socket.h>

#include "sema.h"

#define SEMA_DEBUG

void SemaPrintThreadIds(sema_t *sema)
{
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
}

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

int SemaGetPriority(sema_t *sema, int *priority, int *position)
{
  int i, j;
  for (i = 0; i < SEMA_NUM_PRIORITIES; i++)
  {
    for (j = 0; j < SEMA_NUM_PER_PRIORITY; j++)
    {
      if (sema->waiting_thread_status[i][j])
      {
        if (pthread_equal(pthread_self(), sema->waiting_thread[i][j]))
        {
          *priority = i;
          *position = j;
#ifdef SEMA_DEBUG
          printf("Found prio %d pos %d thread %lu %lu cmp %d\n",
                 i, j, pthread_self(), sema->waiting_thread[i][j],
                 pthread_equal(pthread_self(), sema->waiting_thread[i][j]));
#endif
          return (sema->waiting_thread_status[i][j]);
        }
      }
    }
  }
  return -1;
}

int SemaSetPriorityInternal(sema_t *sema, pthread_t waitThread, int priority,
                            int status_to_set)
{
  int j;

  for (j = 0; j < SEMA_NUM_PER_PRIORITY; j++)
  {

    // Check if priority is set for a dead thread
    if (sema->waiting_thread_status[priority][j] == SEMA_STATUS_PRIOITY_SET)
    {
      if (pthread_kill(sema->waiting_thread[priority][j], 0) == ESRCH)
      {
#ifdef SEMA_DEBUG
        printf("Old thread %lu is dead, reset\n", sema->waiting_thread[priority][j]);
#endif
        sema->waiting_thread_status[priority][j] = 0;
        memset(&sema->waiting_thread[priority][j], 0, sizeof(pthread_t));
      }
    }

    if (!sema->waiting_thread_status[priority][j])
    {
      sema->waiting_thread_status[priority][j] = status_to_set;
      memcpy(&sema->waiting_thread[priority][j], &waitThread, sizeof(pthread_t));
#ifdef SEMA_DEBUG
      printf("SetPriority %d pos %d for id %lu (check %lu)\n", priority, j,
             waitThread, sema->waiting_thread[priority][j]);
      SemaPrintThreadIds(sema);
#endif
      return j;
    }
  }

  return -1;
}

int sema_set_priority(sema_t *sema, int priority)
{
  int priority_a;
  int position_a;
  int status_to_set;

  status_to_set = SEMA_STATUS_PRIOITY_SET;

  if (SemaGetPriority(sema, &priority_a, &position_a) < 0)
  {
    return SemaSetPriorityInternal(sema, pthread_self(), priority, status_to_set);
  }
  else
  {
    if (priority_a == priority)
    {
#ifdef SEMA_DEBUG
      printf("SetPriority unchanged at prio %d pos %d \n",
             priority_a, position_a);
#endif
      return position_a;
    }
    else
    {
      printf("SetPriority: change %d-->%d\n", priority_a, priority);
      // keep the old status, just change priority
      status_to_set = sema->waiting_thread_status[priority_a][position_a];
      sema->waiting_thread_status[priority_a][position_a] = 0;
      memset(&sema->waiting_thread[priority_a][position_a], 0, sizeof(pthread_t));
      return SemaSetPriorityInternal(sema, pthread_self(), priority, status_to_set);
    }
  }
}

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

int SemaSetWait(sema_t *sema, int priority, int position)
{

  if (sema->waiting_thread_status[priority][position] == SEMA_STATUS_PRIOITY_SET)
  {
    sema->waiting_thread_status[priority][position] = SEMA_STATUS_PRIOITY_WAIT;
    return 1;
  }
  return -1;
}

int SemaUnsetWait(sema_t *sema, int priority, int position)
{

  if (sema->waiting_thread_status[priority][position] == SEMA_STATUS_PRIOITY_WAIT)
  {
    sema->waiting_thread_status[priority][position] = SEMA_STATUS_PRIOITY_SET;
    return 1;
  }
  return -1;
}

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
#ifdef SEMA_DEBUG
    printf("sema_wait: semval %d proceed. Numwaiting %d. Status %d\n",
           sema->semval + 1, sema->numWaiting, sema->waiting_thread_status[priority][position]);
    SemaPrintThreadIds(sema);
#endif
    pthread_mutex_unlock(&sema->semLock);
    return;
  }
  else
  {
    sema->numWaiting++;
#ifdef SEMA_DEBUG
    printf("sema_wait: wait semval %d prio %d pos %d numwaiting %d %lu stat %d\n",
           sema->semval, priority, position,
           sema->numWaiting, pthread_self(),
           sema->waiting_thread_status[priority][position]);
    SemaPrintThreadIds(sema);
#endif
    sem_init(&sema->priority_sem[priority][position], sema->pshared, 0);
    pthread_mutex_unlock(&sema->semLock);
    sem_wait(&sema->priority_sem[priority][position]);
    sema->numWaiting--;
    sema->semval--;
#ifdef SEMA_DEBUG
    printf("sema_wait: got wait semval %d prio %d pos %d numwaiting %d %lu stat %d\n",
           sema->semval, priority, position,
           sema->numWaiting, pthread_self(),
           sema->waiting_thread_status[priority][position]);
#endif
    sema->waiting_thread_status[priority][position] = 0;
    memset(&sema->waiting_thread[priority][position], 0, sizeof(pthread_t));
  }
  return;
}

void sema_post(sema_t *sema)
{
  int priority;
  int position;

  pthread_mutex_lock(&sema->semLock);
  if (SemaGetPost(sema, &priority, &position) < 0)
  {
    // Error? No waiting threads found
    sema->semval++;
#ifdef SEMA_DEBUG
    printf("sema_post: No waiting threads found. Semval %d. nm %d\n",
           sema->semval, sema->numWaiting);
    SemaPrintThreadIds(sema);
#endif
    pthread_mutex_unlock(&sema->semLock);
    return;
  }
  else
  {
#ifdef SEMA_DEBUG
    printf("sema_post: Found waiting thread prio %d pos %d ", priority, position);
#endif
  }

  sema->semval++;
  SemaUnsetWait(sema, priority, position);
#ifdef SEMA_DEBUG
  SemaPrintThreadIds(sema);
  printf("sema_post: prepost semval %d numwaiting %d\n", sema->semval, sema->numWaiting);
#endif
  sem_post(&sema->priority_sem[priority][position]);
  pthread_mutex_unlock(&sema->semLock);
}

int sema_getvalue(sema_t *sema, int *semval)
{
  *semval = sema->semval;
  return 0;
}
