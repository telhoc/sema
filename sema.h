#ifndef _SEMA_H
#define _SEMA_H

#include <semaphore.h>

//Priority semaphore threads
#define SEMA_NUM_PRIORITIES 6 //MAX PRIORITY is 5
#define SEMA_NUM_PER_PRIORITY 5
#define SEMA_STATUS_PRIOITY_SET 1
#define SEMA_STATUS_PRIOITY_WAIT 2
#define SEMA_STATUS_PRIOITY_RELEASED 3

typedef struct sema_list_t {

  int             priority_rr_pos[SEMA_NUM_PRIORITIES];
  sem_t           priority_sem[SEMA_NUM_PRIORITIES][SEMA_NUM_PER_PRIORITY];
  pthread_t       waiting_thread[SEMA_NUM_PRIORITIES][SEMA_NUM_PER_PRIORITY];
  int             waiting_thread_status[SEMA_NUM_PRIORITIES][SEMA_NUM_PER_PRIORITY];
  int             semval;
  int             numWaiting;
  int             pshared;
  pthread_mutex_t semLock;
} sema_t;

void sema_init(sema_t *sema, int pshared, unsigned int value);
int sema_set_priority(sema_t *sema, int priority);
void sema_wait(sema_t *sema);
void sema_post(sema_t *sema);
int sema_getvalue(sema_t *sema, int *semval);

#endif
