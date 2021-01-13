// Standard libraries
// #include <stdio.h> //if you don't use scanf/printf change this include (INCLUDED IN data_strucures.h)
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

// Custom files
#include "data_structures.h"
#include "constants.h"


typedef short bool;
#define true 1
#define false 0

#define SHKEY 300

///==============================
//don't mess with this variable//
int *shmaddr; //
//===============================

int getClk()
{
    return *shmaddr;
}

/*
 * All process call this function at the beginning to establish communication between them and the clock module.
 * Again, remember that the clock is only emulation!
*/
void initClk()
{
    int shmid = shmget(SHKEY, 4, 0444);
    while ((int)shmid == -1)
    {
        //Make sure that the clock exists
        printf("Wait! The clock not initialized yet!\n");
        sleep(1);
        shmid = shmget(SHKEY, 4, 0444);
    }
    shmaddr = (int *)shmat(shmid, (void *)0, 0);
}

/*
 * All process call this function at the end to release the communication
 * resources between them and the clock module.
 * Again, Remember that the clock is only emulation!
 * Input: terminateAll: a flag to indicate whether that this is the end of simulation.
 *                      It terminates the whole system and releases resources.
*/

void destroyClk(bool terminateAll)
{
    shmdt(shmaddr);
    if (terminateAll)
    {
        killpg(getpgrp(), SIGINT);
    }
}

/*
 * This gets the down queue ID that sends the process information
 * from the process generator to the scheduler
 * If it is already created just get the ID without new creation
*/

int getProcessDownQueue(int callingProcess)

{

    key_t key_id;

    int msgq_id;

    key_id = ftok(PROCESS_Q_FILE_NAME, PROCESS_Q_UNIQUE_NUM);

    msgq_id = msgget(key_id, 0666 | IPC_CREAT);

    char p1[] = "PG";
    if (callingProcess == 1)
        printf("%s::MsgQ Created with id: %d\n", p1, msgq_id);

    return msgq_id;
}
