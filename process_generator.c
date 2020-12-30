#include "headers.h"

void clearResources(int);
ProcessStaticInfo *readInputFile();
int getSchedulingAlgorithm(int *quantum);
void createClockAndScheduler(int schedulingAlgorithm, int quantum);

// global variables to clean in INT handler
int processDownQueueId;
ProcessStaticInfo *processes;

int main(int argc, char *argv[])
{
    signal(SIGINT, clearResources);
    // TODO Initialization
    // 1. Read the input file.
    int processesNumber;
    processes = readInputFile(&processesNumber);

    // 2. Ask the user for the chosen scheduling algorithm and its parameters, if there are any.
    int quantum = 0;
    int schedulingAlgorithm = getSchedulingAlgorithm(&quantum);
    // 3. Initiate and create the scheduler and clock processes.
    createClockAndScheduler(schedulingAlgorithm, quantum);

    // 4. Use this function after creating the clock process to initialize clock
    initClk();
    // To get time use this
    // int x = getClk();
    // printf("current time is %d\n", x);

    // 5 create message queue to use in communication with scheduler
    processDownQueueId = getProcessDownQueue();
    unsigned long sizeOfMessage = sizeof(ProcessStaticInfo) - sizeof(long);
    int processToSend = 0;
    int time;

    // TODO Generation Main Loop
    // 6. Create a data structure for processes and provide it with its parameters.
    // 7. Send the information to the scheduler at the appropriate time.

    while (processToSend < processesNumber)
    {

        time = getClk();

        while (processes[processToSend].arrivalTime <= time)
        {
            // send process

            // create message
            // 3 is arbitary value as long as it's not 0
            processes[processToSend].mType = 3;

            // send message
            msgsnd(processDownQueueId, &processes[processToSend], sizeOfMessage, !IPC_NOWAIT);

            ++processToSend;
        }
    }

    // sleep waiting for a signal to terminate
    sigset_t emptySet;
    sigemptyset(&emptySet);
    sigsuspend(&emptySet);

    return 0;
}

void clearResources(int signum)
{
    //TODO Clears all resources in case of interruption
    msgctl(processDownQueueId, IPC_RMID, (struct msqid_ds *)0);
    free(processes);
    destroyClk(true);
    exit(0);
}

ProcessStaticInfo *readInputFile(int *numberOfProcesses)
{
    // Keep asking user for input file name till enter a valid file name
    char fileName[50];
    do
    {
        printf("\nEnter name of input file: ");
        scanf("%s", fileName);

    } while (access(fileName, F_OK) != 0);

    // First get number of processes in file
    FILE *inputFile = fopen(fileName, "r");

    char buff[255];

    *numberOfProcesses = 0;

    while (fgets(buff, sizeof(buff), inputFile) != NULL)
    {
        if (buff[0] >= '0' && buff[0] <= '9')
        {
            // this means it is a process line
            ++(*numberOfProcesses);
        }
    }

    fclose(inputFile);

    // increase number of processes to have a dummy process
    ++(*numberOfProcesses);

    // create array to hold all processes static info
    ProcessStaticInfo *processes = malloc(*numberOfProcesses * sizeof(ProcessStaticInfo));

    // read file again to get data of all processes
    inputFile = fopen(fileName, "r");

    int processNumber = 0;

    while (fgets(buff, sizeof(buff), inputFile) != NULL)
    {
        if (buff[0] >= '0' && buff[0] <= '9')
        {
            // now at a line of process
            // track where at in row
            int locationInRow = 0;

            // first get id of process
            int processId = 0;
            while (buff[locationInRow] != '\t')
            {

                processId *= 10;
                processId += buff[locationInRow] - '0';

                ++locationInRow;
            }
            ++locationInRow;

            // set the process id
            processes[processNumber].id = processId;

            // next get the process arrival time
            int arrivalTime = 0;
            while (buff[locationInRow] != '\t')
            {

                arrivalTime *= 10;
                arrivalTime += buff[locationInRow] - '0';

                ++locationInRow;
            }
            ++locationInRow;

            processes[processNumber].arrivalTime = arrivalTime;

            // next get the run time
            int runTime = 0;
            while (buff[locationInRow] != '\t')
            {

                runTime *= 10;
                runTime += buff[locationInRow] - '0';

                ++locationInRow;
            }
            ++locationInRow;

            processes[processNumber].runTime = runTime;

            // finally get the priority
            int priority = 0;
            while ((buff[locationInRow] >= '0') && (buff[locationInRow] <= '9'))
            {

                priority *= 10;
                priority += buff[locationInRow] - '0';

                ++locationInRow;
            }

            processes[processNumber].priority = priority;

            // increment for next loop to access correct array position
            ++processNumber;
        }
    }

    fclose(inputFile);

    // set id and arrival time of dummy process to -1
    processes[(*numberOfProcesses) - 1].id = -1;
    processes[(*numberOfProcesses) - 1].arrivalTime = -1;

    return processes;
}

int getSchedulingAlgorithm(int *quantum)
{
    // keep asking user for algorithm number till enter a valid input
    int schedulingAlgorithm;
    do
    {
        printf("\nChoose Scheduling Algorithm: 1-2-3\n");
        printf("1-> Non-preemtive Highest Priority First (HPF)\n");
        printf("2-> Shortest Remaining Time Next (SRTN)\n");
        printf("3-> Round Robin (RR)\n");
        printf("Choice: ");

        scanf("%d", &schedulingAlgorithm);

    } while ((schedulingAlgorithm < 1) || (schedulingAlgorithm > 3));

    // if entry is round robin then get it's quantum
    if (schedulingAlgorithm == 3)
    {
        // keep asking user for quantum till enter a quantum bigger than 0
        do
        {
            printf("\nEnter Quantum: ");
            scanf("%d", quantum);

        } while ((*quantum) < 1);
    }

    return schedulingAlgorithm;
}

void createClockAndScheduler(int schedulingAlgorithm, int quantum)
{
    // first create clock
    pid_t pid = fork();
    if (pid == 0)
    {
        char *argv[] = {"./clk.out", NULL};
        execv("./clk.out", argv);
    }

    // then create scheduler
    pid = fork();
    if (pid == 0)
    {
        // turn scheduling algorithm to string
        char schedulingAlgoStr[4];
        sprintf(schedulingAlgoStr, "%d", schedulingAlgorithm);

        // turn quantum to string
        char quantumStr[4];
        sprintf(quantumStr, "%d", quantum);

        char *argv[] = {"./scheduler.out", schedulingAlgoStr, quantumStr, NULL};
        execv("./scheduler.out", argv);
    }
}
