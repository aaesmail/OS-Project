#include "headers.h"
#include <time.h>
#include "memory.h"

int createProcess(int runTime);
void hpf_scheduler(struct Logger *logs, double *cpu_utilization, int remainingTimeQId);
void rr_scheduler(struct Logger *logs, double *cpu_utilization, int quantum, int remainingTimeQId);
void srtn_scheduler(struct Logger *logs, double *cpu_utilization, int remainingTimeQId);
void processDoneHandler(int sig);
void cleanResources(int sigNum);

bool processDone = false;
int remainingTimeQId;

int main(int argc, char *argv[])
{
    signal(SIGUSR1, processDoneHandler);
    signal(SIGINT, cleanResources);

    printf("SCHEDULER:: init\n");

    initClk();
    initMemory();

    int key_id = ftok("keyfile", 65);
    remainingTimeQId = msgget(key_id, 0666 | IPC_CREAT);

    printf("SCHEDULER::MsgQ (for remaining time) Created with id: %d\n", remainingTimeQId);

    struct Logger logs;

    loggerInit(&logs);
    double cpu_utilization;

    // read which algorithm to run
    int schedulingAlgorithm = atoi(argv[1]);
    switch (schedulingAlgorithm)
    {
    case HPF:
        hpf_scheduler(&logs, &cpu_utilization, remainingTimeQId);
        break;
    case SRTN:
        srtn_scheduler(&logs, &cpu_utilization, remainingTimeQId);
        break;
    case RR:
        rr_scheduler(&logs, &cpu_utilization, atoi(argv[2]), remainingTimeQId);
        break;
    default:
        printf("SCHEDULER:: WRONG PARAMETER FOR THE SCHEDULING ALGORITHM ERRRRRRRRRRRRRRRRRR\n");
        break;
    }

    //Print logs
    printLogger(&logs, cpu_utilization);
    emptyLogger(&logs);

    // clean resources
    cleanResources(3);

    //Exit
    return 0;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------

void processDoneHandler(int sig)
{
    processDone = true;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------

void cleanResources(int sigNum)
{
    //upon termination release the clock resources.
    destroyClk(false);
    destroyMemory();

    // notify process generator that it terminated to clear resources
    kill(getppid(), SIGINT);

    // delete remaianing time msg queue
    msgctl(remainingTimeQId, IPC_RMID, (struct msqid_ds *)0);
    // Exit
    exit(0);
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------

// return the process id (pid)
int createProcess(int runTime)
{
    // converting run time to chars
    char runTime_str[4];
    sprintf(runTime_str, "%d", runTime);

    // fork to execute the new process
    pid_t pid = fork();
    if (pid == 0)
    {
        char *argv[] = {"./process.out", runTime_str, NULL};
        // stoping the new process till the scheduler decide to resume it
        execv("./process.out", argv);
        printf("SCHEDULER:: after execv ERRRRRRRRRRRRRRRRRRRRRRR!!!!!\n");
    }
    // kill(pid, SIGSTOP);
    printf("SCHEDULER:: process %d launched\n", pid);
    // printf("\n\n    pid: %d, runTime: %s, id: %d\n\n", pid, runTime, p->id);
    return pid;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------

void hpf_scheduler(struct Logger *logs, double *cpu_utilization, int remainingTimeQId)
{
    int processesQId = getProcessDownQueue(2);
    unsigned long sizeOfMessage = sizeof(ProcessStaticInfo) - sizeof(long);
    ProcessStaticInfo *process = malloc(sizeof(ProcessStaticInfo)); //To receive messages in.

    // Create empty priority queue
    Pcb *priorityQHead = NULL;
    Pcb *tempPriorityQHead = NULL;

    Log *newLog;

    bool readingDone = false;

    int schedulerStartTime = 0;
    int schedulerFinishTime;
    int schedulerTotalTime;

    int quantumStartTime = 0;
    int quantumFinishTime = schedulerStartTime;
    int schedulerWastedTime = 0;

    struct msgBuff message;
    message.mtype = 0;

    while (!readingDone || !isEmpty(&priorityQHead))
    {
        // checking if there are any new messages from the process generetor.
        while (!readingDone && msgrcv(processesQId, process, sizeOfMessage, 0, IPC_NOWAIT) != -1)
        {
            // When the process generator sends a -1 msg indicating that it finished sending all the processes.
            if (process->id == -1)
            {
                free(process);
                readingDone = true;
            }
            // When the process generator sends a msg other than -1 indicating that a new process has arrived to the system.
            else
            {
                // Insert new Pcb: id=pid, arrivalTime=given arr time, runTime=given run time, remainingTime= given run time, waitingTime=0, priority=given priority, stoppedTime=0(not used in HPF), state=WAITING
                Pcb *newPcb = createPcb(process->id, -1, process->arrivalTime, process->runTime, process->runTime, 0, process->priority, process->memSize, 0, WAITING);
                enqueue(&priorityQHead, newPcb);
            }
        }

        Pcb currentRunningProcess;

        //When the Q is empty but there are processes that haven't come yet.
        if (!dequeue(&priorityQHead, &currentRunningProcess))
            continue;

        quantumStartTime = getClk();

        // loop through the whole priority queue untill you find a process that can be allocated at the meantime
        // save the processes that can't be allocated in a temp queue
        bool scannedAllQ = false;
        while (!scannedAllQ && !allocate(quantumStartTime, currentRunningProcess.id, currentRunningProcess.size))
        {
            Pcb *tempPcb = (Pcb *)malloc(sizeof(Pcb));
            *tempPcb = currentRunningProcess;
            enqueue(&tempPriorityQHead, tempPcb);
            Pcb *temp = dequeue(&priorityQHead, &currentRunningProcess);
            // When pirorityQ is empty
            if (!temp)
                scannedAllQ = true;
        }
        // Now we either have a process ready to run in the variable currentRunningProcess or there are no processes that could fit for allocation.
        // Return all the processes from the temp queue to the priority queue
        while (!isEmpty(&tempPriorityQHead))
        {
            Pcb *tempPcb = (Pcb *)malloc(sizeof(Pcb));
            dequeue(&tempPriorityQHead, tempPcb);
            enqueue(&priorityQHead, tempPcb);
        }

        // If all priority is scanned but no process fits for memory allocation
        // Should never happen for HPF, but god knows!
        if (scannedAllQ)
            continue;

        //Start the scheduled process
        if (currentRunningProcess.pid == -1)
        {
            currentRunningProcess.pid = createProcess(currentRunningProcess.runTime);
        }
        else
            kill(currentRunningProcess.pid, SIGCONT);

        schedulerWastedTime += quantumStartTime - quantumFinishTime;
        currentRunningProcess.state = RUNNING;
        currentRunningProcess.waitingTime += quantumStartTime - currentRunningProcess.arrivalTime; //@ HPF always = quantumStartTime - currentRunningProcess.arrivalTime [The += is the same as =]

        newLog = createLog(
            quantumStartTime,
            currentRunningProcess.id,
            STARTED,
            currentRunningProcess.arrivalTime,
            currentRunningProcess.runTime,
            currentRunningProcess.remainingTime,
            currentRunningProcess.waitingTime);

        logger_enqueue(logs, newLog);

        // printf("SCHEDULER:: started process %d\n", currentRunningProcess.pid);

        int oldTime = getClk();
        int time = getClk();
        message.mtype = currentRunningProcess.pid % 100000;

        while (!processDone)
        {
            time = getClk();
            if (oldTime < time)
            {
                (currentRunningProcess.remainingTime) -= (time - oldTime);
                oldTime = time;
                message.mint = currentRunningProcess.remainingTime;
                msgsnd(remainingTimeQId, &message, sizeof(message.mint), !IPC_NOWAIT);
            }
        }

        quantumFinishTime = time;
        deAllocate(quantumFinishTime, currentRunningProcess.id);
        newLog = createLog(
            quantumFinishTime,
            currentRunningProcess.id,
            FINISHED,
            currentRunningProcess.arrivalTime,
            currentRunningProcess.runTime,
            currentRunningProcess.remainingTime,
            currentRunningProcess.waitingTime);
        logger_enqueue(logs, newLog);

        printf("SCHEDULER:: finished process %d\n", currentRunningProcess.id);
        processDone = false;
    }
    schedulerFinishTime = getClk();
    schedulerTotalTime = schedulerFinishTime - schedulerStartTime;
    (*cpu_utilization) = ((double)(schedulerTotalTime - schedulerWastedTime) / schedulerTotalTime) * 100;
    printf("SCHEDULER:: total time = %d  Wasted time = %d CPU util = %f\n", schedulerTotalTime, schedulerWastedTime, *cpu_utilization);
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------

void rr_scheduler(struct Logger *logs, double *cpu_utilization, int quantum, int remainingTimeQId)
{
    // get the id of message queue that has processes
    int processesQId = getProcessDownQueue(2);
    unsigned long sizeOfMessage = sizeof(ProcessStaticInfo);
    ProcessStaticInfo *process = malloc(sizeof(ProcessStaticInfo)); //To receive messages in.

    // creation of empty circular queue

    circularQueue q;
    q.front = q.rear = NULL;

    // latest log pointer
    Log *newLog;

    // flag used in the next loop to indicate the end of the reading process

    bool readingDone = false;

    // pointer to the pcb of the last process runned
    Pcb *lastRunningProcess = NULL;

    int schedulerStartTime = getClk();
    int schedulerFinishTime;
    int schedulerTotalTime;
    int schedulerWastedTime = 0;

    int quantumStartTime = 0;
    int quantumFinishTime = schedulerStartTime;
    int logState;

    struct msgBuff message;
    message.mtype = 0;

    while (true)
    {
        // checking if there is a new message from the process generetor
        while (!readingDone && msgrcv(processesQId, process, sizeOfMessage, 0, IPC_NOWAIT) != -1)
        {
            // When the process generator sends a -1 msg indicating that it finished sending all the processes.
            if (process->id == -1)
            {
                free(process);
                readingDone = true;
            }
            // When the process generator sends a msg other than -1 indicating that a new process has arrived to the system.
            else
            {
                // if there, add it to the queue
                Pcb *recievedProcess = createPcb(process->id, -1, process->arrivalTime, process->runTime, process->runTime, 0, process->priority, process->memSize, process->arrivalTime, WAITING);
                circularQueue_enqueue(&q, recievedProcess);
            }
        }
        // RR : resume the current in order process for either a quantum or till its done (min. of them)

        if (lastRunningProcess != NULL)
            circularQueue_enqueue(&q, lastRunningProcess);

        lastRunningProcess = circularQueue_dequeue(&q);
        if (!lastRunningProcess && readingDone)
        {
            printf("SCHEDULER:: FINISHED ALLn\n");
            break;
        }

        //When the Q is empty but there are processes that haven't come yet.

        if (!lastRunningProcess)
        {
            continue;
        }

        quantumStartTime = getClk();

        if (lastRunningProcess->pid == -1)
        {
            if (!allocate(quantumStartTime, lastRunningProcess->id, lastRunningProcess->size))
                continue;

            logState = STARTED;
            lastRunningProcess->pid = createProcess(lastRunningProcess->runTime);
        }
        else
        {
            logState = RESUMED;
            kill(lastRunningProcess->pid, SIGCONT);
        }

        lastRunningProcess->state = RUNNING;
        schedulerWastedTime += quantumStartTime - quantumFinishTime;
        lastRunningProcess->waitingTime += quantumStartTime - lastRunningProcess->stoppedTime;

        newLog = createLog(
            quantumStartTime,
            lastRunningProcess->id,
            logState,
            lastRunningProcess->arrivalTime,
            lastRunningProcess->runTime,
            lastRunningProcess->remainingTime,
            lastRunningProcess->waitingTime);

        logger_enqueue(logs, newLog);

        printf("SCHEDULER:: resumed process %d\n", lastRunningProcess->pid);

        int oldTime = getClk();
        int time = getClk();
        message.mtype = lastRunningProcess->pid % 100000;

        while ((quantum > time - quantumStartTime) && !processDone)
        {
            time = getClk();
            if (oldTime < time)
            {
                (lastRunningProcess->remainingTime) -= (time - oldTime);
                oldTime = time;
                message.mint = lastRunningProcess->remainingTime;
                msgsnd(remainingTimeQId, &message, sizeof(message.mint), !IPC_NOWAIT);
                if (lastRunningProcess->remainingTime == 0)
                    while (!processDone)
                        ;
            }
        }

        kill(lastRunningProcess->pid, SIGSTOP);
        quantumFinishTime = getClk();

        printf("SCHEDULER:: stopped process %d\n", lastRunningProcess->pid);

        // printf("pid: %d,    start: %d,     end: %d\n", lastRunningProcess->id, startTime, getClk());

        if (processDone)
        {

            printf("SCHEDULER:: finished process %d\n", lastRunningProcess->pid);
            processDone = false;

            newLog = createLog(
                quantumFinishTime,
                lastRunningProcess->id,
                FINISHED,
                lastRunningProcess->arrivalTime,
                lastRunningProcess->runTime,
                lastRunningProcess->remainingTime,
                lastRunningProcess->waitingTime);

            logger_enqueue(logs, newLog);

            deAllocate(quantumFinishTime, lastRunningProcess->id);
            free(lastRunningProcess);
            lastRunningProcess = NULL;
        }
        else
        {
            lastRunningProcess->stoppedTime = quantumFinishTime;
            lastRunningProcess->state = WAITING;
            newLog = createLog(
                quantumFinishTime,
                lastRunningProcess->id,
                STOPPED,
                lastRunningProcess->arrivalTime,
                lastRunningProcess->runTime,
                lastRunningProcess->remainingTime,
                lastRunningProcess->waitingTime);

            logger_enqueue(logs, newLog);
        }
    }

    schedulerFinishTime = getClk();
    schedulerTotalTime = schedulerFinishTime - schedulerStartTime;
    printf("%d  %d\n", schedulerTotalTime, schedulerWastedTime);
    (*cpu_utilization) = ((double)(schedulerTotalTime - schedulerWastedTime) / schedulerTotalTime) * 100;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------

void srtn_scheduler(struct Logger *logs, double *cpu_utilization, int remainingTimeQId)
{
    int processesQId = getProcessDownQueue(2);
    unsigned long sizeOfMessage = sizeof(ProcessStaticInfo);
    ProcessStaticInfo *process = malloc(sizeof(ProcessStaticInfo)); //To receive messages in.

    // Create empty priority queue
    Pcb *priorityQHead = NULL;
    Log *newLog;

    bool readingDone = false;

    int schedulerStartTime = getClk();
    int schedulerFinishTime;
    int schedulerTotalTime;

    int quantumStartTime = 0;
    int quantumFinishTime = 0;
    int schedulerWastedTime = 0;

    int currentlyRunning = -1;

    int stoppedTime = 0;

    Pcb currentRunningProcess;

    struct msgBuff message;
    message.mtype = 0;

    int time = getClk(), oldTime = getClk();

    while (true)
    {
        time = getClk();
        if (time > oldTime)
        {
            if (currentlyRunning != -1)
            {
                currentRunningProcess.remainingTime -= time - oldTime;
                message.mtype = currentRunningProcess.pid % 100000;
                message.mint = currentRunningProcess.remainingTime;
                msgsnd(remainingTimeQId, &message, sizeof(message.mint), !IPC_NOWAIT);
            }
            oldTime = time;
        }
        // checking if there are any new messages from the process generetor.
        while (!readingDone && msgrcv(processesQId, process, sizeOfMessage, 0, IPC_NOWAIT) != -1)
        {
            // When the process generator sends a -1 msg indicating that it finished sending all the processes.
            if (process->id == -1)
            {
                free(process);
                readingDone = true;
            }
            // When the process generator sends a msg other than -1 indicating that a new process has arrived to the system.

            else
            {
                //printf("New Process with runtime: %d \n\n", process->runTime);
                //printf("pid: %d, runtime: %d, Current time: %d\n\n", process->id, process->runTime, getClk());
                if (currentlyRunning != -1)
                {
                    kill(currentlyRunning, SIGSTOP);
                    quantumFinishTime = getClk();
                    //currentRunningProcess.remainingTime -= getClk() - quantumStartTime;
                    if (currentRunningProcess.remainingTime <= process->runTime)
                    {
                        //Insert new process into queue
                        //printf("Process with id: %d is continuing as its remaining time is: %d\n\n", currentlyRunning, currentRunningProcess.remainingTime);
                        // Insert new Pcb: id=pid, arrivalTime=given arr time, runTime=given run time, remainingTime= given run time, priority=given priority(Run Time for SRTN), state=WAITING
                        Pcb *n = createPcb(process->id, -2, process->arrivalTime, process->runTime, process->runTime, 0, process->runTime, process->memSize, -1, WAITING);
                        enqueue(&priorityQHead, n);
                        quantumStartTime = getClk();

                        kill(currentlyRunning, SIGCONT);
                    }
                    else
                    {
                        // Insert currently running process into queue
                        Pcb *n = createPcb(currentRunningProcess.id, currentRunningProcess.pid, currentRunningProcess.arrivalTime, currentRunningProcess.runTime, currentRunningProcess.remainingTime, currentRunningProcess.waitingTime, currentRunningProcess.priority, process->memSize, quantumFinishTime, WAITING);
                        enqueue(&priorityQHead, n);
                        newLog = createLog(
                            quantumFinishTime,
                            currentRunningProcess.id,
                            STOPPED,
                            currentRunningProcess.arrivalTime,
                            currentRunningProcess.runTime,
                            currentRunningProcess.remainingTime,
                            currentRunningProcess.waitingTime);
                        logger_enqueue(logs, newLog);

                        printf("SCHEDULER:: stopped process %d\n", currentRunningProcess.pid);

                        //printf("Stopping process with id: %d to start process: %d\n\n", currentlyRunning, pid);

                        quantumStartTime = getClk();
                        int pid = createProcess(process->runTime);

                        currentlyRunning = pid;
                        currentRunningProcess.id = process->id;
                        currentRunningProcess.pid = pid;
                        currentRunningProcess.arrivalTime = quantumStartTime;
                        currentRunningProcess.runTime = process->runTime;
                        currentRunningProcess.remainingTime = process->runTime;
                        if (quantumFinishTime != 0)
                            schedulerWastedTime += quantumStartTime - quantumFinishTime;
                        currentRunningProcess.state = RUNNING;
                        currentRunningProcess.waitingTime = quantumStartTime - currentRunningProcess.arrivalTime;

                        newLog = createLog(
                            quantumStartTime,
                            currentRunningProcess.id,
                            STARTED,
                            currentRunningProcess.arrivalTime,
                            currentRunningProcess.runTime,
                            currentRunningProcess.remainingTime,
                            currentRunningProcess.waitingTime);

                        logger_enqueue(logs, newLog);

                        printf("SCHEDULER:: started process %d\n", currentRunningProcess.pid);
                    }
                }
                else
                {
                    int pid = createProcess(process->runTime);
                    quantumStartTime = getClk();
                    currentlyRunning = pid;
                    currentRunningProcess.id = process->id;
                    currentRunningProcess.pid = pid;
                    currentRunningProcess.arrivalTime = quantumStartTime;
                    currentRunningProcess.runTime = process->runTime;
                    currentRunningProcess.remainingTime = process->runTime;
                    currentRunningProcess.priority = process->runTime;
                    currentRunningProcess.stoppedTime = -1;
                    if (quantumFinishTime != 0)
                        schedulerWastedTime += quantumStartTime - quantumFinishTime;
                    currentRunningProcess.state = RUNNING;
                    currentRunningProcess.waitingTime = quantumStartTime - currentRunningProcess.arrivalTime;

                    newLog = createLog(
                        quantumStartTime,
                        currentRunningProcess.id,
                        STARTED,
                        currentRunningProcess.arrivalTime,
                        currentRunningProcess.runTime,
                        currentRunningProcess.remainingTime,
                        currentRunningProcess.waitingTime);

                    logger_enqueue(logs, newLog);

                    printf("SCHEDULER:: started process %d\n", currentRunningProcess.pid);
                }
            }
        }
        if (processDone)
        {
            //printf("PID: %d DONE\n", currentlyRunning);
            currentlyRunning = -1;
            processDone = false;
            quantumFinishTime = getClk();
            //currentRunningProcess.remainingTime -= quantumFinishTime - quantumStartTime;
            newLog = createLog(
                quantumFinishTime,
                currentRunningProcess.id,
                FINISHED,
                currentRunningProcess.arrivalTime,
                currentRunningProcess.runTime,
                currentRunningProcess.remainingTime,
                currentRunningProcess.waitingTime);
            logger_enqueue(logs, newLog);

            printf("SCHEDULER:: finished process %d\n", currentRunningProcess.pid);

            if (!dequeue(&priorityQHead, &currentRunningProcess))
            {
                if (readingDone)
                    break;
                else
                    continue;
            }

            if (currentRunningProcess.pid == -2)
                currentRunningProcess.pid = createProcess(currentRunningProcess.runTime);
            else
                kill(currentRunningProcess.pid, SIGCONT);

            currentlyRunning = currentRunningProcess.pid;

            quantumStartTime = getClk();
            if (quantumFinishTime != 0)
                schedulerWastedTime += quantumStartTime - quantumFinishTime;
            currentRunningProcess.state = RUNNING;
            if (currentRunningProcess.stoppedTime == -1)
                currentRunningProcess.waitingTime += quantumStartTime - currentRunningProcess.arrivalTime;
            else
                currentRunningProcess.waitingTime += quantumStartTime - currentRunningProcess.stoppedTime;

            newLog = createLog(
                quantumStartTime,
                currentRunningProcess.id,
                STARTED,
                currentRunningProcess.arrivalTime,
                currentRunningProcess.runTime,
                currentRunningProcess.remainingTime,
                currentRunningProcess.waitingTime);

            logger_enqueue(logs, newLog);

            printf("SCHEDULER:: started process %d\n", currentRunningProcess.pid);
        }
        if (readingDone && currentlyRunning == -1)
            break;
    }

    schedulerFinishTime = getClk();
    schedulerTotalTime = schedulerFinishTime - schedulerStartTime;
    printf("%d  %d\n", schedulerTotalTime, schedulerWastedTime);
    (*cpu_utilization) = ((double)(schedulerTotalTime - schedulerWastedTime) / schedulerTotalTime) * 100;
}