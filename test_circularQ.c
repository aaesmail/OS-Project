#include "data_structures.h"
#include <stdio.h>

int main()
{

    circularQueue q;
    q.rear = q.front = NULL;
    Pcb *x = (Pcb *)malloc(sizeof(Pcb));
    x->id = 1;

    Pcb *y = (Pcb *)malloc(sizeof(Pcb));
    y->id = 2;

    Pcb *z = (Pcb *)malloc(sizeof(Pcb));
    z->id = 3;

    circularQueue_enqueue(&q, x);
    circularQueue_enqueue(&q, y);
    circularQueue_enqueue(&q, z);

    x = circularQueue_onetick(&q);
    printf("%d\n", x->id);

    x = circularQueue_onetick(&q);
    printf("%d\n", x->id);

    x = circularQueue_onetick(&q);
    printf("%d\n", x->id);

    x = circularQueue_onetick(&q);
    printf("%d\n", x->id);

    x = circularQueue_onetick(&q);
    printf("%d\n", x->id);

    Pcb *k = (Pcb *)malloc(sizeof(Pcb));
    k->id = 132;

    circularQueue_enqueue(&q, k);

    x = circularQueue_onetick(&q);
    printf("%d\n", x->id);

    x = circularQueue_onetick(&q);
    printf("%d\n", x->id);

    x = circularQueue_onetick(&q);
    printf("%d\n", x->id);

    x = circularQueue_onetick(&q);
    printf("%d\n", x->id);

    x = circularQueue_onetick(&q);
    printf("%d\n", x->id);

    x = circularQueue_onetick(&q);
    printf("%d\n", x->id);

    // Pcb* head = createPcb(1,1,1,1,6,1);
    // enqueue(&head, createPcb(2,2,2,2,5,2));
    // enqueue(&head, createPcb(3,3,3,3,4,3));
    // enqueue(&head, createPcb(4,4,4,4,3,4));
    // enqueue(&head, createPcb(6,6,6,6,1,6));
    // enqueue(&head, createPcb(7,7,7,7,1,7));
    // enqueue(&head, createPcb(8,8,8,8,0,8));
    // enqueue(&head, createPcb(5,5,5,5,2,5));



    // printQueue(&head);

    return 0;
}