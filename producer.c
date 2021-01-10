#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <signal.h>

// ID of all IPC to use
#define SHARED_MEMORY_ID 25
#define MESSAGE_ID 26
#define PRODUCTION_LOC_ID 27
#define CONSUMPTION_LOC_ID 28
#define PROTECT_MEM_ID 29
#define FULL_SLOTS_ID 30
#define PROCESSES_NUM_ID 31

#define BUFF_SIZE 20


// message to use in message sending
struct msgbuff
{
    long mtype;
    char mtext;
};


/* arg for semctl system calls. */
union Semun
{
    int val;               /* value for SETVAL */
    struct semid_ds *buf;  /* buffer for IPC_STAT & IPC_SET */
    ushort *array;         /* array for GETALL & SETALL */
    struct seminfo *__buf; /* buffer for IPC_INFO */
    void *__pad;
};


// down the semaphore
void down(int sem)
{
    struct sembuf p_op;

    p_op.sem_num = 0;
    p_op.sem_op = -1;
    p_op.sem_flg = !IPC_NOWAIT;

    if (semop(sem, &p_op, 1) == -1)
    {
        perror("Error in down()");
        exit(-1);
    }
}


// up the semaphore
void up(int sem)
{
    struct sembuf v_op;

    v_op.sem_num = 0;
    v_op.sem_op = 1;
    v_op.sem_flg = !IPC_NOWAIT;

    if (semop(sem, &v_op, 1) == -1)
    {
        perror("Error in up()");
        exit(-1);
    }
}


// get semaphore value
int get_semaphore(int semId, union Semun arg) {
    return semctl(semId, 0, GETVAL, arg);
}


// get semaphore if it exists
// creates semaphore and initializes it if it exists
int create_semaphore(int ID, int init_val) {
    int sem_id = semget(ID, 1, 0666 | IPC_CREAT | IPC_EXCL);
    union Semun semun;
    if (sem_id == -1) {
        // already exists
        sem_id = semget(ID, 1, 0666 | IPC_CREAT);
    }
    else {
        // i created it
        semun.val = init_val;
        semctl(sem_id, 0, SETVAL, semun);
    }
    return sem_id;
}


// put these variables here to use them in cleaning resources
int msgq_id, shmid, production_loc, consumption_loc, protect_mem, full_slots, processes_num;
int* shmaddr;
union Semun semun;


// clean resources after getting killed
void cleanResources(int sigNum) {

    // tell all resources that i am exiting
    down(processes_num);

    if (get_semaphore(processes_num, semun) == 0) {
        // i am the last process
        // so clear resources
        // remove message queue
        msgctl(msgq_id, IPC_RMID, (struct msqid_ds *)0);
        // remove shared memory
        shmctl(shmid, IPC_RMID, (struct shmid_ds *)0);
        // remove semaphores
        semctl(production_loc, 0, IPC_RMID, semun);
        semctl(consumption_loc, 0, IPC_RMID, semun);
        semctl(protect_mem, 0, IPC_RMID, semun);
        semctl(full_slots, 0, IPC_RMID, semun);
        semctl(processes_num, 0, IPC_RMID, semun);
    }
    else {
        // there are still processes left so just detach from memory
        shmdt(shmaddr);
    }

    // exit
    exit(0);
}


int main() {

    // increment processes number
    processes_num = create_semaphore(PROCESSES_NUM_ID, 0);
    up(processes_num);


    // put sig int callback to clean resources in it
    signal(SIGINT, cleanResources);


    // variables to use
    struct msgbuff message;
    message.mtype = 7;


    // id of message queue to send and receive messages
    msgq_id = msgget(MESSAGE_ID, 0666 | IPC_CREAT);

    
    // id of shared memory segment
    shmid = shmget(SHARED_MEMORY_ID, BUFF_SIZE * sizeof(int), IPC_CREAT | 0644);
    shmaddr = shmat(shmid, 0, 0);


    // id of production location so far
    production_loc = create_semaphore(PRODUCTION_LOC_ID, 0);


    // id of consumption location so far
    consumption_loc = create_semaphore(CONSUMPTION_LOC_ID, 0);


    // id of the binary semaphore that protects access to memory
    protect_mem = create_semaphore(PROTECT_MEM_ID, 1);


    // id of semaphore that tell how many items are full in buffer
    full_slots = create_semaphore(FULL_SLOTS_ID, 0);


    // producer produces above the buffer size by 30
    for (int i = 0; i < BUFF_SIZE + 30; ++i) {
        // if the number of slots is buffer size
        // then wait until the consumer consumes something
        if (get_semaphore(full_slots, semun) == BUFF_SIZE) {
            while (msgrcv(msgq_id, &message, sizeof(message.mtext), 8, !IPC_NOWAIT) == -1);
        }

        // enter critical section
        down(protect_mem);

        // get number of item to produce
        int loc = get_semaphore(production_loc, semun);
        // put item in buffer
        shmaddr[loc % BUFF_SIZE] = loc;
        // print about producer item
        printf("Produced item: %d\tat loc: %d\n", loc, loc % BUFF_SIZE);

        // if full slots was 0 then tell consumer that i inserted an item
        if (get_semaphore(full_slots, semun) == 0) {
            message.mtype = 7;
            msgsnd(msgq_id, &message, sizeof(message.mtext), !IPC_NOWAIT);
        }

        up(full_slots);
        up(production_loc);
        up(protect_mem);
    }

    // clean all resources
    cleanResources(3);

    return 0;
}
