#include "headers.h"
int createProcess(ProcessStaticInfo *p);
bool processDone = false;

void processDoneHandler(int sig)
{
    processDone = true;
}

int main(int argc, char *argv[])
{
    signal(SIGUSR1, processDoneHandler);

    initClk();

    //TODO implement the scheduler :)

    // read which algorithm to run
    int schedulingAlgorithm = atoi(argv[1]);

    // get the id of message queue that has processes

    int processesQId = getProcessDownQueue();
    unsigned long sizeOfMessage = sizeof(ProcessStaticInfo);
    ProcessStaticInfo *process = malloc(sizeof(ProcessStaticInfo));

    if (schedulingAlgorithm == HPF)
    {
        // Highest priority first
    }
    else if (schedulingAlgorithm == SRTN)
    {
        // Shortest remaining time next
    }
    else if (schedulingAlgorithm == RR)
    {
        int quantum = atoi(argv[2]);
        circularQueue q;
        q.front = q.rear = NULL;
        node *n;
        bool readingDone = false;

        while (true)
        {
            // checking if there is a new process from the process generetor
            if (!readingDone && msgrcv(processesQId, process, sizeOfMessage, 0, IPC_NOWAIT) != -1)

                if (process->id == -1)
                {
                    free(process);
                    readingDone = true;
                }
                else
                {
                    // if there, add it to the queue

                    int pid = createProcess(process);
                    n = malloc(sizeof(node));
                    n->id = pid;
                    circularQueue_enqueue(&q, n);
                }

            // RR : resume the current in order process for either a quantum or till its done (min. of them)

            n = circularQueue_dequeue(&q);

            if (!n && readingDone)
                break;

            if (!n)
                continue;

            int startTime = getClk();

            kill(n->id, SIGCONT);

            while ((quantum > getClk() - startTime) && !processDone)
                ;

            kill(n->id, SIGSTOP);

            // printf("pid: %d,    start: %d,     end: %d\n\n", n->id, startTime, getClk());

            if (processDone)
            {
                // printf("PID: %d DONE\n", n->id);

                processDone = false;
                free(n);
            }
            else
                circularQueue_enqueue(&q, n);
        }
    }

    //upon termination release the clock resources.
    destroyClk(false);
    // notify process generator that it terminated to clear resources
    kill(getppid(), SIGINT);

    return 0;
}

// return the process id (pid)

int createProcess(ProcessStaticInfo *p)
{
    // converting run time to chars

    char runTime[4];
    sprintf(runTime, "%d", p->runTime);

    pid_t pid = fork();

    // fork to execute the new process

    if (pid == 0)
    {
        char *argv[] = {"./process.out", runTime, NULL};

        // stoping the new process till the scheduler decide to resume it

        raise(SIGSTOP);
        execv("./process.out", argv);
    }

    // printf("\n\n    pid: %d, runTime: %s, id: %d\n\n", pid, runTime, p->id);
    return pid;
}
