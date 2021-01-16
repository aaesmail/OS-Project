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

    // Create empty priority queue for ready processes
    Pcb *priorityQHead = NULL;
    Log *newLog;

    // Create empty priority queue for waiting processes
    Pcb *waitingPriorityQHead = NULL;

    bool readingDone = false;

    int schedulerStartTime = getClk();
    int schedulerFinishTime;
    int schedulerTotalTime;
    int schedulerWastedTime = 0;

    int quantumStartTime = 0;
    int quantumFinishTime = schedulerStartTime;

    int currentlyRunning = -1;
    int stoppedTime = 0;

    Pcb currentRunningProcess;

    struct msgBuff message;
    message.mtype = 0;

    bool newProcessEntered = false;
    Pcb *tempPcb;

    int time = getClk(), oldTime = getClk();

    while (true)
    {
        time = getClk();
        // If a clock tick has passed
        if (time > oldTime)
        {
            // If there is a running process
            if (currentlyRunning != -1)
            {
                // Decrement its running time and send a message to the process
                // with its new remaining tim
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
            // When the process generator sends a -1 msg indicating that it finished
            // sending all the processes.
            if (process->id == -1)
            {
                free(process);
                readingDone = true;
            }
            // When the process generator sends a msg other than -1 indicating that a new process
            // has arrived to the system.
            else
            {
                // Insert process in waiting queue
                Pcb *n = createPcb(process->id, -2, process->arrivalTime, process->runTime, process->runTime, 0, process->runTime, process->memSize, -1, WAITING);
                enqueue(&waitingPriorityQHead, n);
                newProcessEntered = true;
            }
        }
        if (newProcessEntered)
        {
            newProcessEntered = false;
            tempPcb = (Pcb *)malloc(sizeof(Pcb));
            // Get the process with least remaining time
            dequeue(&waitingPriorityQHead, tempPcb);

            // If there is a process currently running
            if (currentlyRunning != -1)
                // If the new process has less remaining time and can be allocated
                if (tempPcb->remainingTime < currentRunningProcess.remainingTime && allocate(getClk(), tempPcb->id, tempPcb->size))
                {
                    // Stop the currently running process
                    kill(currentlyRunning, SIGSTOP);
                    quantumFinishTime = getClk();
                    // Insert currently running process into ready queue
                    Pcb *n = createPcb(currentRunningProcess.id, currentRunningProcess.pid, currentRunningProcess.arrivalTime, currentRunningProcess.runTime, currentRunningProcess.remainingTime, currentRunningProcess.waitingTime, currentRunningProcess.remainingTime, currentRunningProcess.size, quantumFinishTime, WAITING);
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
                    // Create the new process and start it
                    quantumStartTime = getClk();
                    int pid = createProcess(tempPcb->runTime);

                    currentlyRunning = pid;
                    currentRunningProcess.id = tempPcb->id;
                    currentRunningProcess.pid = pid;
                    currentRunningProcess.arrivalTime = quantumStartTime;
                    currentRunningProcess.runTime = tempPcb->runTime;
                    currentRunningProcess.remainingTime = tempPcb->runTime;
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
                    free(tempPcb);
                }
                else
                {
                    // If the new process has a greater remaining time or can't be allocated
                    // Insert new process into waiting queue
                    enqueue(&waitingPriorityQHead, tempPcb);
                }
            // If no process is currently running
            else
            {
                allocate(getClk(), tempPcb->id, tempPcb->size);
                // Create the new process and start it
                int pid = createProcess(tempPcb->runTime);
                quantumStartTime = getClk();

                currentlyRunning = pid;
                currentRunningProcess.id = tempPcb->id;
                currentRunningProcess.pid = pid;
                currentRunningProcess.arrivalTime = quantumStartTime;
                currentRunningProcess.runTime = tempPcb->runTime;
                currentRunningProcess.remainingTime = tempPcb->runTime;
                currentRunningProcess.priority = tempPcb->runTime;
                currentRunningProcess.stoppedTime = -1;
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
                free(tempPcb);
            }
        }
        // If the currently running process has finished
        if (processDone)
        {
            currentlyRunning = -1;
            processDone = false;
            quantumFinishTime = getClk();

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
            // Deallocating the finished process.
            deAllocate(quantumFinishTime, currentRunningProcess.id);

            tempPcb = (Pcb *)malloc(sizeof(Pcb));
            // If the waiting queue isn't empty get its first item in tempPcb
            if (dequeue(&waitingPriorityQHead, tempPcb))
                // If ready queue isn't empty
                if (priorityQHead)
                    // Check if it has a higher priority (less remaining time) than the first one in the ready queue
                    // Then Check if it could be allocated in memory
                    if (tempPcb->priority < priorityQHead->priority && allocate(quantumFinishTime, tempPcb->id, tempPcb->size))
                        // Add it to ready queue
                        enqueue(&priorityQHead, tempPcb);
                    else
                        // Return it to waiting queue
                        enqueue(&waitingPriorityQHead, tempPcb);
                // If ready queue is empty allocate it and add it to ready queue
                else{
                    allocate(quantumFinishTime, tempPcb->id, tempPcb->size);
                    enqueue(&priorityQHead, tempPcb);
                }
            else
                free(tempPcb);

            // Check if the ready queue is empty
            if (!dequeue(&priorityQHead, &currentRunningProcess))
            {
                // If it's empty and no other processes will enter then break;
                if (readingDone)
                    break;
                // Else continue until another process enters
                else
                    continue;
            }
            // If we hadn't created this process before (It wasn't run before)
            if (currentRunningProcess.pid == -2)
                // create and start this process
                currentRunningProcess.pid = createProcess(currentRunningProcess.runTime);
            else
                // If it was already created then send it a continue signal
                kill(currentRunningProcess.pid, SIGCONT);

            currentlyRunning = currentRunningProcess.pid;
            quantumStartTime = getClk();
            schedulerWastedTime += quantumStartTime - quantumFinishTime;
            currentRunningProcess.state = RUNNING;

            // If the current process hadn't been preempted before
            if (currentRunningProcess.stoppedTime == -1)
                // Its waiting time is calculated since its arrival
                currentRunningProcess.waitingTime += quantumStartTime - currentRunningProcess.arrivalTime;

            // If it was preempted
            else
                // Then its waiting time is increased by the time since it was stopped
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

        // If no process is currently running and no new process will enter then exit
        // This case will only happen when no processes are sent at all
        // As we always check whenever a process finishes
        if (readingDone && currentlyRunning == -1)
            break;
    }

    schedulerFinishTime = getClk();
    schedulerTotalTime = schedulerFinishTime - schedulerStartTime;
    printf("%d  %d\n", schedulerTotalTime, schedulerWastedTime);
    (*cpu_utilization) = ((double)(schedulerTotalTime - schedulerWastedTime) / schedulerTotalTime) * 100;
}