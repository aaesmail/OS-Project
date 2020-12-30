
// struct to hold static data of processes
#include <stdlib.h>

typedef struct
{

    long mType;
    int id;
    int arrivalTime;
    int runTime;
    int priority;

} ProcessStaticInfo;

typedef struct node
{
    int id;
    struct node *next;
} node;

typedef struct
{
    node *front;
    node *rear;
} circularQueue;

void circularQueue_enqueue(circularQueue *q, node *current)
{

    node *last = q->rear;

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

node *circularQueue_onetick(circularQueue *q)
{
    node *current = q->front;

    if (current)
        q->front = current->next;

    return current;
}

node *circularQueue_dequeue(circularQueue *q)
{
    node *current = q->front;

    if (current && current != current->next)
        q->front = current->next;
    else
        q->front = NULL;

    if (q->rear)
        q->rear->next = q->front;

    return current;
}