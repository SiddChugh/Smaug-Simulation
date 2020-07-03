#include <errno.h>
#include <wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <curses.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h> // library for interprocessing communication 
#include <sys/sem.h> // library for semaphores
#include <sys/shm.h> // library for shared memory
#include <sys/time.h>
#include <sys/resource.h>

/**
  *Define semaphores to be placed in a single semaphore set
  *Numbers indicate index in semaphore set for named semaphore
  */
#define SEM_COWSINGROUP 0
#define SEM_PCOWSINGROUP 1
#define SEM_SHEEPSINGROUP 2
#define SEM_PSHEEPSINGROUP 3
#define SEM_COWSWAITING 7
#define SEM_SHEEPSWAITING 8
#define SEM_PSHEEPSEATEN 9
#define SEM_SHEEPSEATEN 10
#define SEM_PCOWSEATEN 11
#define SEM_COWSEATEN 12
#define SEM_SHEEPSDEAD 15
#define SEM_COWSDEAD 16
#define SEM_PTERMINATE 17
#define SEM_DRAGONEATING 19
#define SEM_DRAGONFIGHTING 20
#define SEM_DRAGONSLEEPING 21
#define SEM_DRAGONSWIMMING 22
#define SEM_PMEALWAITINGFLAG 23

/**
  *System constants used to control simulation termination
  */
#define MAX_SHEEPS_EATEN 14
#define MAX_COWS_EATEN 14
#define MAX_COWS_CREATED 80
#define MAX_DEFEATED_TREASURE 12
#define MAX_DEFEATED_THIEF 15
#define MAX_TREASURE_IN_HOARD 800
#define MIN_TREASURE_IN_HOARD 0

/**
  *System constants for initialization
  */
#define INITIAL_TREASURE_IN_HOARD 400

/**
  *System constants to specify size of groups of cows
  */
#define COWS_IN_GROUP 2
#define SHEEPS_IN_GROUP 2

// CREATING YOUR SEMAPHORES
int semID;

union semun
{
  int val;
  struct semid_ds *buf;
  ushort *array;
} seminfo;

struct timeval startTime;

/**
  *Pointers and ids for shared memory segments
  */
int *terminateFlagp = NULL;
int *sheepCounterp = NULL;
int *sheepsEatenCounterp = NULL;
int *cowCounterp = NULL;
int *cowsEatenCounterp = NULL;
int *mealWaitingFlagp = NULL;
int terminateFlag = 0;
int sheepCounter = 0;
int sheepsEatenCounter = 0;
int cowCounter = 0;
int cowsEatenCounter = 0;
int mealWaitingFlag = 0;

/**
  *Group IDs for managing/removing processes
  */
int smaugProcessID = -1;
int sheepProcessGID = -1;
int cowProcessGID = -1;
int parentProcessGID = -1;

/**
  *Number in group semaphores
  * -1 = decrement with Wait, 1 = increment with Signal
  */
struct sembuf WaitCowsInGroup = {SEM_COWSINGROUP, -1, 0};
struct sembuf SignalCowsInGroup = {SEM_COWSINGROUP, 1, 0};
struct sembuf WaitSheepsInGroup = {SEM_SHEEPSINGROUP, -1, 0};
struct sembuf SignalSheepsInGroup = {SEM_SHEEPSINGROUP, 1, 0};
struct sembuf WaitProtectCowsInGroup = {SEM_PCOWSINGROUP, -1, 0};
struct sembuf SignalProtectCowsInGroup = {SEM_PCOWSINGROUP, 1, 0};
struct sembuf WaitProtectSheepsInGroup = {SEM_PSHEEPSINGROUP, -1, 0};
struct sembuf SignalProtectSheepsInGroup = {SEM_PSHEEPSINGROUP, 1, 0};
struct sembuf WaitProtectMealWaitingFlag = {SEM_PMEALWAITINGFLAG, -1, 0};
struct sembuf SignalProtectMealWaitingFlag = {SEM_PMEALWAITINGFLAG, 1, 0};
struct sembuf WaitCowsWaiting = {SEM_COWSWAITING, -1, 0};
struct sembuf SignalCowsWaiting = {SEM_COWSWAITING, 1, 0};
struct sembuf WaitSheepsWaiting = {SEM_SHEEPSWAITING, -1, 0};
struct sembuf SignalSheepsWaiting = {SEM_SHEEPSWAITING, 1, 0};
struct sembuf WaitCowsEaten = {SEM_COWSEATEN, -1, 0};
struct sembuf SignalCowsEaten = {SEM_COWSEATEN, 1, 0};
struct sembuf WaitSheepsEaten = {SEM_SHEEPSEATEN, -1, 0};
struct sembuf SignalSheepsEaten = {SEM_SHEEPSEATEN, 1, 0};
struct sembuf WaitProtectCowsEaten = {SEM_PCOWSEATEN, -1, 0};
struct sembuf SignalProtectCowsEaten = {SEM_PCOWSEATEN, 1, 0};
struct sembuf WaitProtectSheepsEaten = {SEM_PSHEEPSEATEN, -1, 0};
struct sembuf SignalProtectSheepsEaten = {SEM_PSHEEPSEATEN, 1, 0};
struct sembuf WaitCowsDead = {SEM_COWSDEAD, -1, 0};
struct sembuf SignalCowsDead = {SEM_COWSDEAD, 1, 0};
struct sembuf WaitSheepsDead = {SEM_SHEEPSDEAD, -1, 0};
struct sembuf SignalSheepsDead = {SEM_SHEEPSDEAD, 1, 0};
struct sembuf WaitDragonEating = {SEM_DRAGONEATING, -1, 0};
struct sembuf SignalDragonEating = {SEM_DRAGONEATING, 1, 0};
struct sembuf WaitDragonFighting = {SEM_DRAGONFIGHTING, -1, 0};
struct sembuf SignalDragonFighting = {SEM_DRAGONFIGHTING, 1, 0};
struct sembuf WaitDragonSleeping = {SEM_DRAGONSLEEPING, -1, 0};
struct sembuf SignalDragonSleeping = {SEM_DRAGONSLEEPING, 1, 0};
struct sembuf WaitDragonSwimming = {SEM_DRAGONSWIMMING, -1, 0};
struct sembuf SignalDragonSwimming = {SEM_DRAGONSWIMMING, -1, 0};
struct sembuf WaitProtectTerminate = {SEM_PTERMINATE, -1, 0};
struct sembuf SignalProtectTerminate = {SEM_PTERMINATE, 1, 0};


double timeChange(struct timeval starttime);
void initialize();
void smaug();
void cow(int startTimeN);
void sheep(int startTimeN);
void terminateSimulation();
void releaseSemandMem();
void semopChecked(int semaphoreID, struct sembuf *operation,
                  unsigned something);
void semctlChecked(int semaphoreID, int semNum, int flag, union semun seminfo);

void smaug()
{
  int k;
  int temp;
  int localpid;
  double elapsedTime;

  /**
    * local counters used only for smaug routine
    */
  int cowsEatenTotal = 0;
  int sheepsEatenTotal = 0;
  int jewels = INITIAL_TREASURE_IN_HOARD;
  int treasureDefeatedTotal = 0;
  int thievesDefeatedTotal = 0;
  int cowsEatenCurrent = 0;
  int sheepsEatenCurrent = 0;

  smaugProcessID = getpid();
  printf("SMAUG PID is %d \n", smaugProcessID);
  localpid = smaugProcessID;
  printf("SMAUG Smaug has gone to sleep\n");
  semopChecked(semID, &WaitDragonSleeping, 1);
  printf("SMAUG Smaug has woken up \n" );
  while (TRUE)
  {
    cowsEatenCurrent = 0;
    sheepsEatenCurrent = 0;
    semopChecked(semID, &WaitProtectMealWaitingFlag, 1);
    while ( * mealWaitingFlagp >= 1 &&
            (cowsEatenCurrent + sheepsEatenCurrent) < 2)
    {
      *mealWaitingFlagp = *mealWaitingFlagp - 1;
      printf("SMAUG signal meal flag %d\n", *mealWaitingFlagp);
      semopChecked(semID, &SignalProtectMealWaitingFlag, 1);
      printf("SMAUG Smaug is eating a meal\n");
      for (k = 0; k < SHEEPS_IN_GROUP; k++)
      {
        semopChecked(semID, &SignalSheepsWaiting, 1);
        printf("SMAUG A sheep is ready to eat\n");
      }
      for (k = 0; k < COWS_IN_GROUP; k++)
      {
        semopChecked(semID, &SignalCowsWaiting, 1);
        printf("SMAUG A cow is ready to eat\n");
      }

      semopChecked(semID, &WaitDragonEating, 1);
      for (k = 0; k < COWS_IN_GROUP; k++)
      {
        semopChecked(semID, &SignalCowsDead, 1);
        cowsEatenTotal++;
        printf("SMAUG Smaug finished eating a cow\n");
      }
      for (k = 0; k < SHEEPS_IN_GROUP; k++)
      {
        semopChecked(semID, &SignalSheepsDead, 1);
        sheepsEatenTotal++;
        printf("SMAUG Smaug finished eating a sheep\n");
      }

      printf("SMAUG Smaug has finished a meal\n");

      if (cowsEatenTotal >= MAX_COWS_EATEN)
      {
        printf("SMAUG Smaug has eaten the allowed number of cows\n");
        *terminateFlagp = 1;
        break;
      }
      if (sheepsEatenTotal >= MAX_SHEEPS_EATEN)
      {
        printf("SMAUG Smaug has eaten the allowed number of sheeps\n");
        *terminateFlagp = 1;
        break;
      }

      semopChecked(semID, &WaitProtectMealWaitingFlag, 1);
      if ( *mealWaitingFlagp > 0)
      {
        printf("SMAUG Smaug eats again\n");
        // printf("SMAUG Smaug eats again\n", localpid);
        continue;
      }
      else
      {
        semopChecked(semID, &SignalProtectMealWaitingFlag, 1);
        printf("SMAUG Smaug sleeps again\n");
        // printf("SMAUG Smaug sleeps again\n", localpid);
        semopChecked(semID, &WaitDragonSleeping, 1);
        printf("SMAUG Smaug is awake again\n");
        // printf("SMAUG Smaug is awake again\n", localpid);
        break;
      }
    }
  }
}

void initialize()
{
  semID = semget(IPC_PRIVATE, 25, 0666 | IPC_CREAT);
  seminfo.val = 0;

  semctlChecked(semID, SEM_COWSINGROUP, SETVAL, seminfo);
  semctlChecked(semID, SEM_COWSWAITING, SETVAL, seminfo);
  semctlChecked(semID, SEM_COWSEATEN, SETVAL, seminfo);
  semctlChecked(semID, SEM_COWSDEAD, SETVAL, seminfo);
  semctlChecked(semID, SEM_SHEEPSINGROUP, SETVAL, seminfo);
  semctlChecked(semID, SEM_SHEEPSWAITING, SETVAL, seminfo);
  semctlChecked(semID, SEM_SHEEPSEATEN, SETVAL, seminfo);
  semctlChecked(semID, SEM_COWSDEAD, SETVAL, seminfo);
  semctlChecked(semID, SEM_DRAGONFIGHTING, SETVAL, seminfo);
  semctlChecked(semID, SEM_DRAGONSLEEPING, SETVAL, seminfo);
  semctlChecked(semID, SEM_DRAGONEATING, SETVAL, seminfo);
  printf("INIT semaphores initiialized\n");

  seminfo.val = 1;
  semctlChecked(semID, SEM_PCOWSINGROUP, SETVAL, seminfo);
  semctlChecked(semID, SEM_PSHEEPSINGROUP, SETVAL, seminfo);
  semctlChecked(semID, SEM_PMEALWAITINGFLAG, SETVAL, seminfo);
  semctlChecked(semID, SEM_PCOWSEATEN, SETVAL, seminfo);
  semctlChecked(semID, SEM_PSHEEPSEATEN, SETVAL, seminfo);
  semctlChecked(semID, SEM_PTERMINATE, SETVAL, seminfo);
  printf("INIT mutexes initiialized\n");


  if ((terminateFlag = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666)) < 0)
  {
    printf("INIT shm not created for terminateFlag\n");
    exit(1);
  }
  else
  {
    printf("INIT shm created for terminateFlag\n");
  }

  if ((cowCounter = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666)) < 0)
  {
    printf("INIT shm not created for cowCounter\n");
    exit(1);
  }
  else
  {
    printf("INIT shm created for cowCounter\n");
  }

  if ((sheepCounter = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666)) < 0
  {
    printf("INIT shm not created for sheepCounter\n");
    exit(1);
  }
  else
  {
    printf("INIT shm created for sheepCounter\n");
  }

  if ((mealWaitingFlag = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666))
      < 0)
  {
    printf("INIT shm not created for mealWaitingFlag\n");
    exit(1);
  }
  else
  {
    printf("INIT shm created for mealWaitingFlag\n");
  }

  if ((cowsEatenCounter = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666))
      < 0)
  {
    printf("INIT shm not created for cowsEatenCounter\n");
    exit(1);
  }
  else
  {
    printf("INIT shm created for cowsEatenCounter\n");
  }

  if ((sheepsEatenCounter = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666))
      < 0)
  {
    printf("INIT shm not created for sheepsEatenCounter\n");
    exit(1);
  }
  else
  {
    printf("INIT shm created for sheepsEatenCounter\n");
  }

  if ((terminateFlagp = shmat(terminateFlag, NULL, 0)) == (int *) - 1)
  {
    printf("INIT shm not attached for terminateFlag\n");
    exit(1);
  }
  else
  {
    printf("INIT shm attached for terminateFlag\n");
  }

  if ((cowCounterp = shmat(cowCounter, NULL, 0)) == (int *) - 1)
  {
    printf("INIT shm not attached for cowCounter\n");
    exit(1);
  }
  else
  {
    printf("INIT shm attached for cowCounter\n");
  }

  if ((sheepCounterp = shmat(sheepCounter, NULL, 0)) == (int *) - 1)
  {
    printf("INIT shm not attached for sheepCounter\n");
    exit(1);
  }
  else
  {
    printf("INIT shm attached for sheepCounter\n");
  }

  if ((mealWaitingFlagp = shmat(mealWaitingFlag, NULL, 0)) == (int *) - 1)
  {
    printf("INIT shm not attached for mealWaitingFlag\n");
    exit(1);
  }
  else
  {
    printf("INIT shm attached for mealWaitingFlag\n");
  }

  if ((cowsEatenCounterp = shmat(cowsEatenCounter, NULL, 0)) == (int *) - 1)
  {
    printf("INIT shm not attached for cowsEatenCounter\n");
    exit(1);
  }
  else
  {
    printf("INIT shm attached for cowsEatenCounter\n");
  }

  if ((sheepsEatenCounterp = shmat(sheepsEatenCounter, NULL, 0)) == 
      (int *) - 1)
  {
    printf("INIT shm not attached for sheepsEatenCounter\n");
    exit(1);
  }
  else
  {
    printf("INIT shm attached for sheepsEatenCounter\n");
  }

  printf("INIT initialize end\n");
}

void sheep(int startTimeN)
{
  int localpid = getpid();

  printf("SHEEP %8d SHEEP A sheep is born\n", localpid);
  if ( startTimeN > 0)
  {
    if ( usleep( startTimeN) == -1) {
      // exit when usleep interrupted by kill signal
      if (errno == EINTR)exit(4);
    }
  }
  printf("SHEEP %8d SHEEP sheep grazes for %f ms\n", localpid,
         startTimeN / 1000.0);

  semopChecked(semID, &WaitProtectSheepsInGroup, 1);
  semopChecked(semID, &SignalSheepsInGroup, 1);
  *sheepCounterp = *sheepCounterp + 1;
  printf("SHEEP %8d SHEEP %d sheeps have been enchanted \n", localpid,
         *sheepCounterp);

  if (*sheepCounterp >= SHEEPS_IN_GROUP)
  { // if there is enough sheeps to be eaten by Smaug
    for (int k = 0; k < SHEEPS_IN_GROUP; k++)
    {
      semopChecked(semID, &WaitSheepsInGroup, 1);
    }

    *sheepCounterp = *sheepCounterp - SHEEPS_IN_GROUP;
    printf("SHEEP %8d SHEEP The last sheep is waiting\n", localpid);
    semopChecked(semID, &SignalProtectSheepsInGroup, 1);
    semopChecked(semID, &WaitProtectMealWaitingFlag, 1);
    *mealWaitingFlagp = *mealWaitingFlagp + 1;
    printf("SHEEP %d SHEEP signal meal flag %d\n", localpid,
           *mealWaitingFlagp);
    semopChecked(semID, &SignalProtectMealWaitingFlag, 1);
    semopChecked(semID, &SignalDragonSleeping, 1);
    printf("SHEEP %d SHEEP last sheep wakes the dragon \n", localpid);
  }
  else
  {
    semopChecked(semID, &SignalProtectSheepsInGroup, 1);
  }
  // this sheep is now waiting to be eaten
  semopChecked(semID, &WaitSheepsWaiting, 1);
  // once Smaug has signal SheepsWaiting and unblocked this sheep
  semopChecked(semID, &WaitProtectSheepsEaten, 1);

  semopChecked(semID, &SignalSheepsEaten, 1);
  *sheepsEatenCounterp = *sheepsEatenCounterp + 1;

  if (*sheepsEatenCounterp >= SHEEPS_IN_GROUP)
  {
    for (int k = 0; k < SHEEPS_IN_GROUP; k++)
    {
      semopChecked(semID, &WaitSheepsEaten, 1);
    }

    *sheepsEatenCounterp = *sheepsEatenCounterp - SHEEPS_IN_GROUP;
    printf("SHEEP %d SHEEP The last sheep has been eaten\n", localpid);
    semopChecked(semID, &SignalProtectSheepsEaten, 1);
    semopChecked(semID, &SignalDragonEating, 1);
  }
  else
  {
    semopChecked(semID, &SignalProtectSheepsEaten, 1);
    printf("SHEEP %8d SHEEP A sheep is waiting to be eaten\n", localpid);
  }
  semopChecked(semID, &WaitSheepsDead, 1);

  printf("SHEEP %d SHEEP Sheep dies", localpid);
}

void cow(int startTimeN)
{
  int localpid = getpid();

  printf("COW %8d COW A cow is born\n", localpid);
  if ( startTimeN > 0)
  {
    if ( usleep( startTimeN) == -1) {
      // exit when usleep interrupted by kill signal
      if (errno == EINTR)exit(4);
    }
  }
  printf("COW %8d COW cow grazes for %f ms\n", localpid, startTimeN / 1000.0);

  semopChecked(semID, &WaitProtectCowsInGroup, 1);
  semopChecked(semID, &SignalCowsInGroup, 1);
  *cowCounterp = *cowCounterp + 1;
  printf("COW %8d COW %d cows have been enchanted \n", localpid, *cowCounterp);

  if (*cowCounterp >= COWS_IN_GROUP)
  {
    for (int k = 0; k < COWS_IN_GROUP; k++) {
      semopChecked(semID, &WaitCowsInGroup, 1);
    }

    *cowCounterp = *cowCounterp - COWS_IN_GROUP;
    printf("COW %8d COW The last cow is waiting\n", localpid);
    semopChecked(semID, &SignalProtectCowsInGroup, 1);
    semopChecked(semID, &WaitProtectMealWaitingFlag, 1);
    *mealWaitingFlagp = *mealWaitingFlagp + 1;
    printf("COW %8d COW signal meal flag %d\n", localpid, *mealWaitingFlagp);
    semopChecked(semID, &SignalProtectMealWaitingFlag, 1);
    semopChecked(semID, &SignalDragonSleeping, 1);
    printf("COW %8d COW last cow wakes the dragon \n", localpid);
  }
  else
  {
    semopChecked(semID, &SignalProtectCowsInGroup, 1);
  }

  semopChecked(semID, &WaitCowsWaiting, 1);
  semopChecked(semID, &WaitProtectCowsEaten, 1);
  semopChecked(semID, &SignalCowsEaten, 1);
  *cowsEatenCounterp = *cowsEatenCounterp + 1;

  if (*cowsEatenCounterp >= COWS_IN_GROUP)
  {
    for (int k = 0; k < COWS_IN_GROUP; k++)
    {
      semopChecked(semID, &WaitCowsEaten, 1);
    }

    *cowsEatenCounterp = *cowsEatenCounterp - COWS_IN_GROUP;
    printf("COW %8d COW The last cow has been eaten\n", localpid);
    semopChecked(semID, &SignalProtectCowsEaten, 1);
    semopChecked(semID, &SignalDragonEating, 1);
  }
  else
  {
    semopChecked(semID, &SignalProtectCowsEaten, 1);
    printf("COW %8d COW A cow is waiting to be eaten\n", localpid);
  }
  semopChecked(semID, &WaitCowsDead, 1);

  printf("COW %8d COW cow dies\n", localpid);
}

void terminateSimulation()
{
  pid_t localpgid;
  pid_t localpid;
  int w = 0;
  int status;

  localpid = getpid();
  printf("RELEASESEMAPHORES Terminating Simulation from process %8d\n",
         localpgid);
  if (cowProcessGID != (int)localpgid )
  {
    if (killpg(cowProcessGID, SIGKILL) == -1 && errno == EPERM)
    {
      printf("TERMINATE COWS NOT KILLED\n");
    }
    printf("TERMINATE killed cows \n");
  }
  if (sheepProcessGID != (int)localpgid )
  {
    if (killpg(sheepProcessGID, SIGKILL) == -1 && errno == EPERM) 
    {
      printf("TERMINATE SHEEPS NOT KILLED\n");
    }
    printf("TERMINATE killed sheeps \n");
  }
  if (smaugProcessID != (int)localpgid )
  {
    kill(smaugProcessID, SIGKILL);
    printf("TERMINATE killed smaug\n");
  }
  /**
    * -1 = special case (can put PID instead), any process that hasn't had 
    * their status checked; WNOHANG = not a blocking version of pid_t
    */
  while ( (w = waitpid( -1, &status, WNOHANG)) > 1) 
  { 
    printf("REAPED process in terminate %d\n", w);
  }
  releaseSemandMem();
  printf("GOODBYE from terminate\n");
}

void releaseSemandMem()
{
  pid_t localpid;
  int w = 0;
  int status;

  localpid = getpid();

  //should check return values for clean termination
  semctl(semID, 0, IPC_RMID, seminfo);


  // wait for the semaphores
  usleep(2000);
  while ( (w = waitpid( -1, &status, WNOHANG)) > 1)
  {
    printf("REAPED process in terminate %d\n", w);
  }
  printf("\n");
  if (shmdt(terminateFlagp) == -1) 
  {
    printf("RELEASE terminateFlag share memory detach failed\n");
  }
  else 
  {
    printf("RELEASE terminateFlag share memory detached\n");
  }
  if ( shmctl(terminateFlag, IPC_RMID, NULL ))
  {
    printf("RELEASE share memory delete failed %d\n", *terminateFlagp );
  }
  else {
    printf("RELEASE share memory deleted\n");
  }
  if ( shmdt(sheepCounterp) == -1)
  {
    printf("RELEASE sheepCounterp memory detach failed\n");
  }
  else 
  {
    printf("RELEASE sheepCounterp memory detached\n");
  }
  if ( shmctl(sheepCounter, IPC_RMID, NULL ))
  {
    printf("RELEASE sheepCounter memory delete failed\n");
  }
  else 
  {
    printf("RELEASE sheepCounter memory deleted\n");
  }
  if ( shmdt(cowCounterp) == -1)
  {
    printf("RELEASE cowCounterp memory detach failed\n");
  }
  else 
  {
    printf("RELEASE cowCounterp memory detached\n");
  }
  if ( shmctl(cowCounter, IPC_RMID, NULL ))
  {
    printf("RELEASE cowCounter memory delete failed \n");
  }
  else {
    printf("RELEASE cowCounter memory deleted\n");
  }
  if ( shmdt(mealWaitingFlagp) == -1)
  {
    printf("RELEASE mealWaitingFlagp memory detach failed\n");
  }
  else {
    printf("RELEASE mealWaitingFlagp memory detached\n");
  }
  if ( shmctl(mealWaitingFlag, IPC_RMID, NULL ))
  {
    printf("RELEASE mealWaitingFlag share memory delete failed \n");
  }
  else {
    printf("RELEASE mealWaitingFlag share memory deleted\n");
  }
  if ( shmdt(sheepsEatenCounterp) == -1)
  {
    printf("RELEASE sheepsEatenCounterp memory detach failed\n");
  }
  else 
  {
    printf("RELEASE sheepsEatenCounterp memory detached\n");
  }
  if ( shmctl(sheepsEatenCounter, IPC_RMID, NULL ))
  {
    printf("RELEASE sheepsEatenCounter memory delete failed\n");
  }
  else {
    printf("RELEASE sheepsEatenCounter memory detached\n");
  }
  if ( shmdt(cowsEatenCounterp) == -1)
  {
    printf("RELEASE cowsEatenCounterp memory detach failed\n");
  }
  else {
    printf("RELEASE cowsEatenCounterp memory detached\n");
  }
  if ( shmctl(cowsEatenCounter, IPC_RMID, NULL ))
  {
    printf("RELEASE cowsEatenCounter memory delete failed \n");
  }
  else {
    printf("RELEASE cowsEatenCounter memory deleted\n");
  }

}

void semctlChecked(int semaphoreID, int semNum, int flag, union semun seminfo) {
  /** 
    *wrapper that checks if the semaphore control request has terminated
    *successfully. If it has not the entire simulation is terminated 
    */
  if (semctl(semaphoreID, semNum, flag,  seminfo) == -1 ) {
    if (errno != EIDRM) 
    {
      printf("semaphore control failed: simulation terminating\n");
      printf("errno %8d \n", errno );
      *terminateFlagp = 1;
      releaseSemandMem();
      exit(2);
    }
    else 
    {
      exit(3);
    }
  }
}

void semopChecked(int semaphoreID, struct sembuf *operation, 
                                   unsigned something)
{

  /**
    *wrapper that checks if the semaphore operation request has terminated 
    *successfully. If it has not the entire simulation is terminated 
    */
  if (semop(semaphoreID, operation, something) == -1 ) 
  {
    if (errno != EIDRM) 
    {
      printf("semaphore operation failed: simulation terminating\n");
      *terminateFlagp = 1;
      releaseSemandMem();
      exit(2);
    }
    else 
    {
      exit(3);
    }
  }
}


double timeChange( const struct timeval startTime )
{
  struct timeval nowTime;
  double elapsedTime;

  gettimeofday(&nowTime, NULL);
  elapsedTime = (nowTime.tv_sec - startTime.tv_sec) * 1000.0;
  elapsedTime +=  (nowTime.tv_usec - startTime.tv_usec) / 1000.0;
  return elapsedTime;
}

int main()
{
  initialize();

  double sheepTimer = 0;
  double cowTimer = 0;

  /* Prompt user to enter seed number*/
  int seed;
  printf("Please enter a random seed to start the simulation: ");
  scanf("%d", &seed);
  srand(seed);
  // printf("You have entered in %d for seed \n", seed);

  int maxCowInt;
  printf("Please enter a maximum interval length for cow (ms): ");
  scanf("%d", &maxCowInt);
  // printf("You have entered in %d for maximum interval length \n", maxCowInt);

  int maxSheepInt;
  printf("Please enter a maximum interval length for sheep (ms): ");
  scanf("%d", &maxSheepInt);
  // printf("You have entered in %d for maximum interval length \n", maxSheepInt);

  parentProcessGID = getpid();
  smaugProcessID = -1;
  sheepProcessGID = parentProcessGID - 1;
  cowProcessGID = parentProcessGID - 2;

  pid_t childPID = fork();
  if (childPID < 0) 
  {
    printf("FORK FAILED\n");
    return 1;
  } else if (childPID = 0) 
  {
    smaug();
    return 0;
  }

  smaugProcessID = childPID;
  gettimeofday(&startTime, NULL);



  while (*terminateFlagp == 0) 
  { 
    // while no termination conditions have been met
    double duration = timeChange(startTime);
    if (sheepTimer <= duration) 
    {
      int childPID = fork();
      if (childPID == 0) 
      { 
        // if child process, create a sheep
        sheep(duration);
        return 0;
      }
    }
  }

  terminateSimulation();
  return 0;
}