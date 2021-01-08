typedef short bool;
#define true 1
#define false 0

/*
 * This function should be called in the begining of the program
 * to initialize the memory and all of it's components.
 * (Will be called at start of process_generator)
*/
void initMemory();


/*
 * This function allocates memory for a cerain process
 * Allows for several allocations by same process
 * Doesn't allow for allocations of sizes 0 or bigger than memory size
 * 
 * params:
 *      time: current time to be used for logging
 *      processId: id of process to allocate for
 *      size: size to allocate in memory
 * 
 * Returns:
 *      true: in case allocation was successful
 *      false: in case there was not enough space currently so try
 *              again another time
*/
bool allocate(int time, int processId, int size);


/*
 * This function deAllocates memory that was allocated for a process
 * before (to be called when a process in leaving the system)
 * 
 * params:
 *      time: current time to be used for logging
 *      processId: id of process to deallocate it's memory
*/
void deAllocate(int time, int processId);


/*
 * This function removes all memory references of memory module
 * should be called at the end of all processes.
 * (Should be called at the end of scheduler)
*/
void destroyMemory();
