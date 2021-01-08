#include "memory.h"
#include <stdio.h>

FILE* memoryLogFile;

/*
 * Grabbing all resources that module needs
*/
void initMemory() {
    memoryLogFile = fopen("memory.log", "w");
    fprintf(memoryLogFile, "#At time x allocated y bytes for process z from i to j\n");
}

/*
 * Releasing all resources
*/
void destroyMemory() {
    fclose(memoryLogFile);
    printf("memory.log saved.\n");
}

void logMemoryAllocation(int time, int size, int processId, int start, int end) {
    fprintf(memoryLogFile, "At time %d allocated %d bytes for process %d from %d to %d\n",
                time, size, processId, start, end);
}

void logMemoryDeAllocation(int time, int size, int processId, int start, int end) {
    fprintf(memoryLogFile, "At time %d freed %d bytes from process %d from %d to %d\n",
                time, size, processId, start, end);
}

bool allocate(int time, int processId, int size) {

}

void deAllocate(int time, int processId) {

}
