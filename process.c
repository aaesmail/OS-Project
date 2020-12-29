#include "headers.h"


int main(int agrc, char * argv[])
{
    initClk();
    
    //TODO it needs to get the remaining time from somewhere
    int remainingtime = atoi(argv[1]);

    // get time
    int old_time = getClk();
    int time;

    while (remainingtime > 0)
    {
        time = getClk();

        if (old_time != time) {
            old_time = time;
            --remainingtime;
        }
    }
    
    destroyClk(false);

    // TODO here it should notify the scheduler
    
    return 0;
}
