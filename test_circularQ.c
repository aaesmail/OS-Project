#include "data_structures.h"
#include <stdio.h>

int main()
{

    circularQueue q;
    q.rear = q.front = NULL;
    node *x = (node *)malloc(sizeof(node));
    x->id = 1;

    node *y = (node *)malloc(sizeof(node));
    y->id = 2;

    node *z = (node *)malloc(sizeof(node));
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

    node *k = (node *)malloc(sizeof(node));
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

    return 0;
}