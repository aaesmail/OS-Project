#include "headers.h"

struct msgBuff
{
    long mtype;
    int mint;
};

/* Modify this file as needed*/
// int remainingtime = 1;

int main(int agrc, char *argv[])
{
    int remainingtime = atoi(argv[1]);

    initClk();
    int key_id = ftok("keyfile", 65);
    int q_id = msgget(key_id, 0666 | IPC_CREAT);

    struct msgBuff message;
    message.mtype = getpid() % 100000;

    while (remainingtime > 0)
    {
        msgrcv(q_id, &message, sizeof(message.mint), message.mtype, !IPC_NOWAIT);
        remainingtime = message.mint;
    }

    destroyClk(false);

    return 0;
}
