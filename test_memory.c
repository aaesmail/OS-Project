#include <stdlib.h>
#include <stdio.h>

#include "memory.h"

int main() {

    initMemory();

    allocate(0, 1, 100);
    allocate(1, 2, 50);
    allocate(2, 3, 300);
    allocate(3, 4, 70);

    if (allocate(4, 5, 512)) {
        printf("done\n");
    }
    else {
        printf("couldn't\n");
    }

    deAllocate(10, 1);

    if (allocate(4, 5, 512)) {
        printf("done\n");
    }
    else {
        printf("couldn't\n");
    }

    deAllocate(11, 2);

        if (allocate(4, 5, 512)) {
        printf("done\n");
    }
    else {
        printf("couldn't\n");
    }

    deAllocate(12, 3);
    deAllocate(13, 4);

    deAllocate(14, 5);

    destroyMemory();

    return 0;
}