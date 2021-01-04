#include "headers.h"

int main(int agrc, char *argv[])
{
    initClk();

    //TODO it needs to get the remaining time from somewhere
    int remainingtime = atoi(argv[1]);

    // get time
    int old_time = getClk();
    int time;
    printf("PROCESS:: %d started\n", getpid());
    while (remainingtime > 0)
    {
        time = getClk();

        if (old_time != time)
        {
            old_time = time;
            --remainingtime;
            printf("PROCESS:: %d has remaining time %d\n", getpid(), remainingtime);
        }
    }

    destroyClk(false);
    printf("PROCESS:: %d finished \n", getppid());

    // TODO here it should notify the scheduler
    kill(getppid(), SIGUSR1);
    return 0;
}
