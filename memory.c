#include "memory.h"
#include <stdio.h>
#include <stdlib.h>

// define memory start end end bytes
#define MEMORY_START 0
#define MEMORY_END 1023

// status of memory block
enum MemoryBlockStatus {
    FREE,
    ALLOCATED,
    HAS_CHILDREN
};

// memory block that hold info about process it holds and size
struct MemoryBlock {

    // status of memory block
    enum MemoryBlockStatus status;

    // start location
    int start;
    // end location
    int end;
    // size
    int size;

    // id of holding process
    int processId;
    // size of holding process
    int processSize;

    // pointers to left and right children
    struct MemoryBlock* left;
    struct MemoryBlock* right;
};
typedef struct MemoryBlock MemoryBlock;


// head of tree of memory
MemoryBlock* head;

// file to write logs to
FILE* memoryLogFile;


/*
 * Grabbing all resources that module needs
*/
void initMemory() {
    // open logging file and write to it first line
    memoryLogFile = fopen("memory.log", "w");
    fprintf(memoryLogFile, "#At time x allocated y bytes for process z from i to j\n");

    // allocate the head of the memory tree and give it the
    // size of memory
    head = malloc(sizeof(MemoryBlock));

    head->status = FREE;
    head->start = MEMORY_START;
    head->end = MEMORY_END;
    head->size = MEMORY_END - MEMORY_START + 1;
    head->processId = -5;

    head->left = NULL;
    head->right = NULL;
}


/*
 * Free all the memory tree
*/
void release_memory(MemoryBlock* block) {
    if (block == NULL) {
        return;
    }

    release_memory(block->left);
    release_memory(block->right);

    free(block);
}


/*
 * Releasing all resources
*/
void destroyMemory() {
    // close logging file
    fclose(memoryLogFile);
    printf("memory.log saved.\n");

    // free the memory tree
    release_memory(head);
    head = NULL;
}


/*
 * log the memory allocation event
*/
void logMemoryAllocation(int time, int size, int processId, int start, int end) {
    fprintf(memoryLogFile, "At time %d allocated %d bytes for process %d from %d to %d\n",
                time, size, processId, start, end);
}


/*
 * log the memory freeing event
*/
void logMemoryDeAllocation(int time, int size, int processId, int start, int end) {
    fprintf(memoryLogFile, "At time %d freed %d bytes from process %d from %d to %d\n",
                time, size, processId, start, end);
}


/*
 * Allocate memory in the tree by assigning a new node to the process
*/
bool allocate_memory(MemoryBlock* block, int size, int processId, int time) {
    if (block == NULL) {
        return false;
    }

    // if process size is bigger than block then it's impossible
    if (block->size < size) {
        return false;
    }

    // if block already holds a process then it's impossible
    if (block->status == ALLOCATED) {
        return false;
    }
    // if block has children then try to allocate in the children
    else if (block->status == HAS_CHILDREN) {

        // try left child first
        if (allocate_memory(block->left, size, processId, time)) {
            return true;
        }

        // if left child failed then try right child
        return allocate_memory(block->right, size, processId, time);
    }
    // if block is free then allocation will succeed
    else if (block->status == FREE) {

        // get child size
        int size_of_child = block->size / 2;

        // if child is smaller than the process size
        // then allcoate this block itself
        if (size_of_child < size) {
            // allocate node
            block->status = ALLOCATED;
            block->processId = processId;
            block->processSize = size;

            logMemoryAllocation(time, size, processId, block->start, block->start + size - 1);
            return true;
        }
        else {
            // then the child is bigger than the process
            // then create left child
            MemoryBlock* left = malloc(sizeof(MemoryBlock));
            left->status = FREE;
            left->start = block->start;
            left->end = left->start + size_of_child - 1;
            left->size = size_of_child;
            left->processId = -5;
            left->left = NULL;
            left->right = NULL;
            
            // and create a right child
            MemoryBlock* right = malloc(sizeof(MemoryBlock));
            right->status = FREE;
            right->start = left->end + 1;
            right->end = right->start + size_of_child - 1;
            right->size = size_of_child;
            right->processId = -5;
            right->left = NULL;
            right->right = NULL;

            // set those children to this block
            block->status = HAS_CHILDREN;
            block->left = left;
            block->right = right;

            // and allocate the process in the left child
            // (or it's children)
            return allocate_memory(block->left, size, processId, time);
        }
    }
}


/*
 * deallocate memory by searching for the process in the tree
 * and if found it then free the node
 * and if found any free neighbouring blocks then join them
*/
void deAllocate_memory(MemoryBlock* block, int processId, int time) {
    if (block == NULL) {
        return;
    }

    // if the block is free then the process is definetly not here
    if (block->status == FREE) {
        return;
    }
    // if the block is allocated then check if it is the process
    else if (block->status == ALLOCATED) {

        // if it is not the process then return
        if (block->processId != processId) {
            return;
        }

        // it is the process so free the block
        block->status = FREE;
        block->processId = -5;

        logMemoryDeAllocation(time, block->processSize, processId, block->start, block->start + block->processSize - 1);

        return;
    }
    // if the block has children then check them
    else if (block->status == HAS_CHILDREN) {
        
        // try to remove the process from the children
        deAllocate_memory(block->left, processId, time);
        deAllocate_memory(block->right, processId, time);

        // if both children became free after operation then merge them
        if ((block->left->status == FREE) && (block->right->status == FREE)) {
            free(block->left);
            free(block->right);

            block->left = NULL;
            block->right = NULL;

            block->status = FREE;

            return;
        }
    }
}


/*
 * Allocates memory block for a process
 * (Doesn't allocate for processes of sizes negative or bigger than memory size)
 * 
 * true: if allocation was success
 * false: if allocation was a failure
*/
bool allocate(int time, int processId, int size) {
    if (size < 0) {
        return false;
    }
    if (size == 0) {
        return true;
    }
    return allocate_memory(head, size, processId, time);
}


/*
 * Remove the process from the memory tree
*/
void deAllocate(int time, int processId) {
    deAllocate_memory(head, processId, time);
}
