#include "headers.h"

int main(int argc, char * argv[])
{
    initClk();
    
    //TODO implement the scheduler :)

    // read which algorithm to run
    int schedulingAlgorithm = atoi(argv[1]);

    // get the id of message queue that has processes
    int processesQId = getProcessDownQueue();

    if (schedulingAlgorithm == HPF) {
        // Highest priority first

    }
    else if (schedulingAlgorithm == SRTN) {
        // Shortest remaining time next

    }
    else if (schedulingAlgorithm == RR) {
        // Round Robin
        int quantum = atoi(argv[2]);

    }

    //upon termination release the clock resources.
    destroyClk(false);
    // notify process generator that it terminated to clear resources
    kill(getppid(), SIGINT);

    return 0;
}
