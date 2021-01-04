#include "headers.h"
#include <time.h>

int createProcess(ProcessStaticInfo *p);
void hpf_scheduler(struct Logger *logs, int *cpu_utilization);
void rr_scheduler(struct Logger *logs, int *cpu_utilization, int quantum);
void srtn_scheduler(struct Logger *logs, int *cpu_utilization);
void processDoneHandler(int sig);

bool processDone = false;

int main(int argc, char *argv[])
{
    signal(SIGUSR1, processDoneHandler);

    initClk();

    //TODO implement the scheduler :)

    time_t start, end;
    // seconds = time(NULL);

    printf("SCHEDULER:: init\n");

    struct Logger logs;
    loggerInit(&logs);
    int cpu_utilization;

    // read which algorithm to run
    int schedulingAlgorithm = atoi(argv[1]);
    switch (schedulingAlgorithm)
    {
    case HPF:
        hpf_scheduler(&logs, &cpu_utilization);
        break;
    case SRTN:
        srtn_scheduler(&logs, &cpu_utilization);
        break;
    case RR:
        rr_scheduler(&logs, &cpu_utilization, atoi(argv[2]));
        break;
    default:
        printf("SCHEDULER:: WRONG PARAMETER FOR THE SCHEDULING ALGORITHM ERRRRRRRRRRRRRRRRRR\n");
        break;
    }

    //Print logs
    printLogger(&logs, cpu_utilization);
    emptyLogger(&logs);

    //upon termination release the clock resources.
    destroyClk(false);

    // notify process generator that it terminated to clear resources
    kill(getppid(), SIGINT);

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

// return the process id (pid)
int createProcess(ProcessStaticInfo *p)
{
    // converting run time to chars
    char runTime[4];
    sprintf(runTime, "%d", p->runTime);

    // fork to execute the new process
    pid_t pid = fork();
    if (pid == 0)
    {
        char *argv[] = {"./process.out", runTime, NULL};
        // stoping the new process till the scheduler decide to resume it
        execv("./process.out", argv);
        printf("SCHEDULER:: after execv ERRRRRRRRRRRRRRRRRRRRRRR!!!!!\n");
    }
    kill(pid, SIGSTOP);
    printf("SCHEDULER:: process %d arrived\n", pid);
    // printf("\n\n    pid: %d, runTime: %s, id: %d\n\n", pid, runTime, p->id);
    return pid;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------

void hpf_scheduler(struct Logger *logs, int *cpu_utilization)
{
    int processesQId = getProcessDownQueue(2);
    unsigned long sizeOfMessage = sizeof(ProcessStaticInfo) - sizeof(long);
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
                int pid = createProcess(process);
                // Insert new Pcb: id=pid, arrivalTime=given arr time, runTime=given run time, remainingTime= given run time, priority=given priority, state=WAITING
                Pcb *newPcb = createPcb(process->id, pid, process->arrivalTime, process->runTime, process->runTime, 0, process->priority, WAITING);
                enqueue(&priorityQHead, newPcb);

            }
        }

        Pcb currentRunningProcess;

        //When the Q is empty but there are processes that haven't come yet.
        if (!dequeue(&priorityQHead, &currentRunningProcess))
            continue;

        quantumStartTime = getClk();
        int x = kill(currentRunningProcess.pid, SIGCONT);

        if(quantumFinishTime != 0) schedulerWastedTime += quantumStartTime - quantumFinishTime;
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

        printf("SCHEDULER:: started process %d\n", currentRunningProcess.pid);

        // while( (getClk() - startTime) > currentRunningProcess.runTime )
        while (!processDone)
            ;
        
        quantumFinishTime = getClk();
        currentRunningProcess.remainingTime -= quantumFinishTime - quantumStartTime; //@ HPF always = 0
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
    printf("%d  %d\n", schedulerTotalTime, schedulerWastedTime);
    (*cpu_utilization) = ((double)(schedulerTotalTime - schedulerWastedTime) / schedulerTotalTime) * 100;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------

void rr_scheduler(struct Logger *logs, int *cpu_utilization, int quantum)
{
    // get the id of message queue that has processes
    int processesQId = getProcessDownQueue(2);
    unsigned long sizeOfMessage = sizeof(ProcessStaticInfo);
    ProcessStaticInfo *process = malloc(sizeof(ProcessStaticInfo)); //To receive messages in.
    circularQueue q;
    q.front = q.rear = NULL;
    Pcb *n;
    bool readingDone = false;
    while (true)
    {
        // checking if there is a new message from the process generetor
        if (!readingDone && msgrcv(processesQId, process, sizeOfMessage, 0, IPC_NOWAIT) != -1)
        {
            // When the process generator sends a -1 msg indicating that it finished sending all the processes.
            if (process->id == -1)
            {
                printf("SCHEDULER:: reading all processes done\n");
                free(process);
                readingDone = true;
            }
            // When the process generator sends a msg other than -1 indicating that a new process has arrived to the system.
            else
            {
                // if there, add it to the queue
                int pid = createProcess(process);
                n = malloc(sizeof(Pcb));
                n->pid = pid;
                circularQueue_enqueue(&q, n);
            }
        }
        // RR : resume the current in order process for either a quantum or till its done (min. of them)
        n = circularQueue_dequeue(&q);
        if (!n && readingDone)
        {
            printf("SCHEDULER:: FINISHED ALLn\n");
            break;
        }

        //When the Q is empty but there are processes that haven't come yet.
        if (!n)
            continue;

        int startTime = getClk();

        kill(n->pid, SIGCONT);
        printf("SCHEDULER:: resumed process %d\n", n->pid);

        while ((quantum > getClk() - startTime) && !processDone)
            ;

        kill(n->pid, SIGSTOP);
        printf("SCHEDULER:: stopped process %d\n", n->pid);

        // printf("pid: %d,    start: %d,     end: %d\n", n->id, startTime, getClk());

        if (processDone)
        {
            printf("SCHEDULER:: finished process %d\n", n->pid);

            processDone = false;
            free(n);
        }
        else
        {
            circularQueue_enqueue(&q, n);
        }
    }
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------

void srtn_scheduler(struct Logger *logs, int *cpu_utilization)
{
    // get the id of message queue that has processes
    int processesQId = getProcessDownQueue(2);
    unsigned long sizeOfMessage = sizeof(ProcessStaticInfo);
    ProcessStaticInfo *process = malloc(sizeof(ProcessStaticInfo)); //To receive messages in.

    while (true)
    {
        //Code Here
    }
}
