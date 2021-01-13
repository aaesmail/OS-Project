
// struct to hold static data of processes
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

typedef struct
{

    long mType;
    int id;
    int arrivalTime;
    int runTime;
    int priority;
    int memSize;

} ProcessStaticInfo;

enum process_state
{
    WAITING,
    RUNNING
};

enum process_log_state
{
    STARTED,
    RESUMED,
    STOPPED,
    FINISHED
};

typedef struct Pcb
{
    int id;  //From text file
    int pid; //process id
    int runTime;
    int remainingTime;
    int waitingTime;
    int arrivalTime;
    int stoppedTime;
    int priority; //Lower value ---> higher priority
    enum process_state state;
    struct Pcb *next;
} Pcb;

typedef struct Log
{
    int time;                     //time --> document (current clk tick)
    int processId;                //process --> document  (ID from text file)
    enum process_log_state state; // state --> document
    int arrivalTime;              //arr --> document
    int runTime;                  //total --> document
    int remainingTime;            //remain --> document
    int waitingTime;              //wait --> document
    struct Log *next;
} Log;

struct msgBuff
{
    long mtype;
    int mint;
};

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------

//CIRCULAR QUEUE FOR PROCESSES TABLE
//-----------------------------------

typedef struct
{
    Pcb *front;
    Pcb *rear;
} circularQueue;

void circularQueue_enqueue(circularQueue *q, Pcb *current)
{

    Pcb *last = q->rear;

    if (last)
    {
        current->next = last->next;
        last->next = current;
    }
    else
    {
        current->next = current;
    }

    if (!q->front)
    {
        q->front = current;
    }

    q->rear = current;

    return;
}

Pcb *circularQueue_onetick(circularQueue *q)
{
    Pcb *current = q->front;

    if (current)
        q->front = current->next;

    return current;
}

Pcb *circularQueue_dequeue(circularQueue *q)
{
    Pcb *current = q->front;

    if (current && current != current->next)
        q->front = current->next;
    else
        q->front = NULL;

    if (q->rear)
        q->rear->next = q->front;

    return current;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------

//PRIORITY QUEUE FOR PROCESSES TABLE
//-----------------------------------
Pcb *createPcb(int id, int pid, int arrivalTime, int runTime, int remainingTime, int waitingTime, int priority, int stoppedTime, enum process_state state)
{

    Pcb *newPcb = (Pcb *)malloc(sizeof(Pcb));
    newPcb->id = id;   //From text file
    newPcb->pid = pid; // process id
    newPcb->arrivalTime = arrivalTime;
    newPcb->runTime = runTime;
    newPcb->remainingTime = remainingTime;
    newPcb->waitingTime = waitingTime;
    newPcb->priority = priority;
    newPcb->stoppedTime = stoppedTime;
    newPcb->state = state;
    newPcb->next = NULL;

    return newPcb;
}

// Function to push according to priority
void enqueue(Pcb **head, Pcb *pcb)
{
    if (!(*head))
    {
        pcb->next = NULL;
        (*head) = pcb;
        return;
    }

    if (pcb->priority < (*head)->priority)
    {
        pcb->next = *head;
        (*head) = pcb;
    }
    else
    {
        Pcb *start = (*head);
        // "<=" is used instead of "<" to handle the tie when a node  comes in with the same priority as another node inside the queue.
        // The older node will be treated as the higher priority.
        while (start->next != NULL && start->next->priority <= pcb->priority)
        {
            start = start->next;
        }
        pcb->next = start->next;
        start->next = pcb;
    }
}

// Return the value at head
Pcb peek(Pcb **head)
{
    return *(*head);
}

// Removes the element with the
// highest priority form the list
Pcb *dequeue(Pcb **head, Pcb *returnValue)
{
    if (!(*head))
        return NULL;
    Pcb *temp = *head;
    (*head) = (*head)->next;
    (*returnValue) = (*temp);
    free(temp);
    return returnValue;
}

void clearQueue(Pcb **head)
{
    while ((*head) != NULL)
    {
        Pcb temp;
        dequeue(head, &temp);
    }
}

// Function to check is list is empty
short isEmpty(Pcb **head)
{
    return (*head) == NULL;
}

//For testing
// #include <stdio.h>

// void printQueue(Pcb **head)
// {
//     while ((*head) != NULL)
//     {
//         printf("id: %d, priority: %d, RemT: %d, runT: %d, state: %d, Wt: %d\n",
//                (*head)->id, (*head)->priority, (*head)->remainingTime, (*head)->runTime, (*head)->state, (*head)->waitingTime);
//         dequeue(head);
//     }
// }

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------

// QUEUE FOR LOGGER
//---------------------------

struct Log *createLog(int time, int processId, enum process_log_state state, int arrivalTime, int runTime, int remainingTime, int waitingTime)
{

    struct Log *newLog = (struct Log *)malloc(sizeof(Log));
    newLog->time = time;
    newLog->processId = processId;
    newLog->state = state;
    newLog->arrivalTime = arrivalTime;
    newLog->runTime = runTime;
    newLog->remainingTime = remainingTime;
    newLog->waitingTime = waitingTime;
    newLog->next = NULL;
    return newLog;
}

struct Logger
{
    struct Log *front;
    struct Log *rear;
};

void loggerInit(struct Logger *logger)
{
    logger->front = NULL;
    logger->rear = NULL;
}

void logger_enqueue(struct Logger *logger, struct Log *log)
{
    if (!(logger->rear)) //If queue is empty !(logger->rear) || !(logger->front)
    {
        log->next = NULL;
        (logger->front) = log;
        (logger->rear) = log;
    }
    else
    {
        logger->rear->next = log;
        log->next = NULL;
        logger->rear = log;
    }
}

double getStdWTA(struct Logger *logger, double avgWTA)
{
    double stdWTA = 0;
    struct Log *currentLog = logger->front;
    int n = 0;
    while (currentLog)
    {
        double f_wta = 0;
        int i_ta = 0;
        if (currentLog->state == FINISHED)
        {
            i_ta = currentLog->waitingTime + currentLog->runTime;
            f_wta = (double)i_ta / currentLog->runTime;
            stdWTA += (avgWTA - f_wta) * (avgWTA - f_wta);
            n++;
        }
        currentLog = currentLog->next;
    }
    if (n == 0)
        return 0;
    return sqrt(stdWTA / n);
}

void removeTrailingZeros(char *str)
{
    char *c = str;
    short foundADot = 0;
    while (*c != '\0')
    {
        if (*c == '.')
            foundADot = 1;
        c++;
    }
    if (!foundADot)
        return;
    c--;
    while (*c == '0')
    {
        *c = '\0';
        c--;
    }
    if (*c == '.')
        *c = '\0';
}

void printLogger(struct Logger *logger, int cpu_utilization)
{

    char s1[] = "started";
    char s2[] = "resumed";
    char s3[] = "stopped";
    char s4[] = "finished";
    char *state;
    char TA_and_WTA[100];
    char out[50];
    int numProcesses = 0;
    int i_ta;
    double f_wta;

    double avgWaiting = 0;
    double avgWTA = 0;
    double stdWTA = 0;

    FILE *log_file = fopen("scheduler.log", "w");

    fprintf(log_file, "#At time x process y state arr w total z remain y wait k\n");
    struct Log *currentLog = logger->front;

    while (currentLog)
    {
        switch (currentLog->state)
        {
        case STARTED:
            state = s1;
            TA_and_WTA[0] = '\0';
            break;
        case RESUMED:
            state = s2;
            TA_and_WTA[0] = '\0';
            break;
        case STOPPED:
            state = s3;
            TA_and_WTA[0] = '\0';
            break;
        case FINISHED:
            state = s4;
            i_ta = currentLog->waitingTime + currentLog->runTime;
            f_wta = (double)i_ta / currentLog->runTime;
            sprintf(TA_and_WTA, " TA %d WTA = %f", i_ta, roundf(f_wta * 100) / 100);
            removeTrailingZeros(TA_and_WTA);
            avgWTA += f_wta;
            avgWaiting += currentLog->waitingTime;
            numProcesses += 1;
            break;
        default:
            printf("UNDEFINED BEHAVIOUR\n");
            exit(-1);
            break;
        }

        //print log
        fprintf(log_file, "At time %d process %d %s arr %d total %d remain %d wait %d%s\n",
                currentLog->time, currentLog->processId, state, currentLog->arrivalTime, currentLog->runTime, currentLog->remainingTime, currentLog->waitingTime, TA_and_WTA);

        currentLog = currentLog->next;
    }

    fclose(log_file);
    printf("scheduler.log saved.\n");

    if (numProcesses != 0)
    {
        avgWaiting /= numProcesses;
        avgWTA /= numProcesses;
    }
    else
    {
        cpu_utilization = 100;
    }

    FILE *perf_file = fopen("scheduler.perf", "w");

    stdWTA = getStdWTA(logger, avgWTA);

    fprintf(perf_file, "CPU utilization = %d%%\n", cpu_utilization);

    sprintf(out, "%.2f", avgWTA);
    removeTrailingZeros(out);

    fprintf(perf_file, "Avg WTA = %s\n", out);

    fprintf(perf_file, "Avg Waiting = %.2f\n", avgWaiting);

    sprintf(out, "%.2f", stdWTA);
    removeTrailingZeros(out);

    fprintf(perf_file, "Std WTA = %s\n", out);

    fclose(perf_file);
    printf("scheduler.perf saved.\n");
}

void emptyLogger(struct Logger *logger)
{

    while (logger->front)
    {
        Log *temp = logger->front;
        logger->front = logger->front->next;
        free(temp);
    }
    logger->rear = NULL;
}
