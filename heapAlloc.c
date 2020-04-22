////////////////////////////////////////////////////////////////////////////////
// Main File:        heapAlloc.c
// This File:        heapAlloc.c
// Other Files:      N/A
// Semester:         CS 354 Spring 2020
//
// Author:           Jeff Hank
// Email:            jhank@wisc.edu
// CS Login:         jhank
//
/////////////////////////// OTHER SOURCES OF HELP //////////////////////////////
//                   Fully acknowledge and credit all sources of help,
//                   other than Instructors and TAs.
//
// Persons:          Identify persons by name, relationship to you, and email.
//                   Describe in detail the the ideas and help they provided.
//
// Online sources:   Avoid web searches to solve your problems, but if you do
//                   search, be sure to include Web URLs and description of
//                   of any information you find.
////////////////////////////////////////////////////////////////////////////////
 
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "heapAlloc.h"
 
/*
 * This structure serves as the header for each allocated and free block.
 * It also serves as the footer for each free block but only containing size.
 */
typedef struct blockHeader {           
    int size_status;
    /*
    * Size of the block is always a multiple of 8.
    * Size is stored in all block headers and free block footers.
    *
    * Status is stored only in headers using the two least significant bits.
    *   Bit0 => least significant bit, last bit
    *   Bit0 == 0 => free block
    *   Bit0 == 1 => allocated block
    *
    *   Bit1 => second last bit 
    *   Bit1 == 0 => previous block is free
    *   Bit1 == 1 => previous block is allocated
    * 
    * End Mark: 
    *  The end of the available memory is indicated using a size_status of 1.
    * 
    * Examples:
    * 
    * 1. Allocated block of size 24 bytes:
    *    Header:
    *      If the previous block is allocated, size_status should be 27
    *      If the previous block is free, size_status should be 25
    * 
    * 2. Free block of size 24 bytes:
    *    Header:
    *      If the previous block is allocated, size_status should be 26
    *      If the previous block is free, size_status should be 24
    *    Footer:
    *      size_status should be 24
    */
} blockHeader;         

/* Global variable - DO NOT CHANGE. It should always point to the first block,
 * i.e., the block at the lowest address.
 */
blockHeader *heapStart = NULL;     

/* Size of heap allocation padded to round to nearest page size.
 */
int allocsize;

/*
 * Additional global variables may be added as needed below
 */
blockHeader *recentlyAllocated = NULL;
 
/* 
 * Function for allocating 'size' bytes of heap memory.
 * Argument size: requested size for the payload
 * Returns address of allocated block on success.
 * Returns NULL on failure.
 * This function should:
 * - Check size - Return NULL if not positive or if larger than heap space.
 * - Determine block size rounding up to a multiple of 8 and possibly adding padding as a result.
 * - Use NEXT-FIT PLACEMENT POLICY to chose a free block
 * - Use SPLITTING to divide the chosen free block into two if it is too large.
 * - Update header(s) and footer as needed.
 * Tips: Be careful with pointer arithmetic and scale factors.
 */
void *allocHeap(int size) {
    //check if size is positive
    if(size < 0) {
        return NULL;
    }
    //check is size is smaller than heap space
    if(size > allocsize) {
        return NULL;
    }
    //determine blocksize
    int blocksize = size + sizeof(blockHeader);
    while(blocksize%8!=0) {
        blocksize+=1;
    }
    //nothing has been allocated yet
    if(recentlyAllocated==NULL) {
        recentlyAllocated = heapStart;
    }
    blockHeader *current = recentlyAllocated;
    //Use next-fit to find a free block
    bool notDone = true;
    while(notDone) {
        int currentSize = current->size_status - current->size_status%4;
        //is the current block free?
        if ((current->size_status & 1) == 0) {
            //is the current free block large enough?
            if (currentSize - blocksize >= 0) {
                //found free block of perfect size
                if (currentSize - blocksize == 0) {
                    current->size_status += 1;
                    recentlyAllocated = current;
                    blockHeader *next = (void*)current + blocksize;
                    next->size_status += 2;
                    notDone = false;
                } else {
                    //split the free block since it's too big
                    int splitSize = currentSize - blocksize;
                    if((current->size_status%8 == 0) && current!=heapStart) {
                        //previous block is free
                        current->size_status = blocksize + 1;
                    } else {
                        //previous block is allocated
                        current->size_status = blocksize + 3;
                    }
                    recentlyAllocated = current;
                    blockHeader *splitBlock = (void*)current + blocksize;
                    blockHeader *footer = (void*)splitBlock + splitSize - 4;
                    splitBlock->size_status = splitSize + 2;
                    footer->size_status = splitSize;
                    notDone = false;
                }
            } else {
                //current free block is too small
                current = (void*)current + currentSize;
            }
        } else {
            //current block is already allocated
            current = (void*)current + currentSize;
            if(current->size_status==1) {
                current = heapStart;
            }
            if(current == recentlyAllocated) {
                return NULL;
            }
        }
    }
    //return the payload
    return (void*)current + sizeof(blockHeader);
}

 
/* 
 * Function for freeing up a previously allocated block.
 * Argument ptr: address of the block to be freed up.
 * Returns 0 on success.
 * Returns -1 on failure.
 * This function should:
 * - Return -1 if ptr is NULL.
 * - Return -1 if ptr is not a multiple of 8.
 * - Return -1 if ptr is outside of the heap space.
 * - Return -1 if ptr block is already freed.
 * - USE IMMEDIATE COALESCING if one or both of the adjacent neighbors are free.
 * - Update header(s) and footer as needed.
 */                    
int freeHeap(void *ptr) {
    //check if pointer is NULL
    if(ptr == NULL) {
        return -1;
    }
    blockHeader *ptrBlock = ptr - sizeof(blockHeader);
    int ptrBlockSize = ptrBlock->size_status - ptrBlock->size_status%4;
    //check if ptr block is freed already
    if ((ptrBlock->size_status & 1) == 0) {
        return -1;
    }
    //check if pointer is not a multiple of 8
    if(ptrBlockSize%8!=0) {
        return -1;
    }
    //check if ptr is outside of heap space
    if(ptrBlock < heapStart || ptrBlock > (heapStart + allocsize)){
        return -1;
    }

    blockHeader *nextBlock = (void*)ptrBlock + ptrBlockSize;
    //if previous block is free
    if ((ptrBlock->size_status % 8 == 1)) {
        blockHeader *prevBlockFooter = (void *) ptrBlock - sizeof(blockHeader);
        blockHeader *prevBlockHeader = (void *) ptrBlock - prevBlockFooter->size_status;
        if(recentlyAllocated==ptrBlock) {
            recentlyAllocated = prevBlockHeader;
        }
        //if next block is also free
        if (nextBlock->size_status % 8 == 2) {
            ptrBlock = prevBlockHeader;
            ptrBlock->size_status += ptrBlockSize + (nextBlock->size_status - nextBlock->size_status % 4);
            prevBlockHeader = NULL;
            prevBlockFooter = (void *) nextBlock + (nextBlock->size_status - nextBlock->size_status % 4) - 4;
            prevBlockFooter->size_status = ptrBlock->size_status - ptrBlock->size_status % 4;
            nextBlock = NULL;

        } else {
            //only previous block is free
            ptrBlock = prevBlockHeader;
            ptrBlock->size_status += ptrBlockSize;
            prevBlockFooter = (void *) nextBlock - 4;
            prevBlockFooter->size_status = ptrBlock->size_status - ptrBlock->size_status % 4;
            nextBlock->size_status -= 2;
            prevBlockHeader = NULL;
        }
    } else if (nextBlock->size_status % 8 == 2) {
        //only next block is free
        ptrBlock->size_status -= 1;
        nextBlock->size_status -= 2;
        ptrBlock->size_status += (nextBlock->size_status - nextBlock->size_status % 4);
        blockHeader *nextFooter = (void *) nextBlock + (nextBlock->size_status - nextBlock->size_status % 4) - 4;
        nextFooter->size_status = ptrBlockSize + (nextBlock->size_status - nextBlock->size_status % 4);
        nextBlock = NULL;
    } else {
        //next block and previous block are both allocated
        ptrBlock->size_status -= 1;
        blockHeader *ptrBlockFooter = (void*) ptrBlock + ptrBlockSize - 4;
        ptrBlockFooter->size_status = ptrBlockSize;
        nextBlock->size_status -= 2;
    }
    return 0;
} 
 
/*
 * Function used to initialize the memory allocator.
 * Intended to be called ONLY once by a program.
 * Argument sizeOfRegion: the size of the heap space to be allocated.
 * Returns 0 on success.
 * Returns -1 on failure.
 */                    
int initHeap(int sizeOfRegion) {    
 
    static int allocated_once = 0; //prevent multiple initHeap calls
 
    int pagesize;  // page size
    int padsize;   // size of padding when heap size not a multiple of page size
    void* mmap_ptr; // pointer to memory mapped area
    int fd;

    blockHeader* endMark;
  
    if (0 != allocated_once) {
        fprintf(stderr, 
        "Error:mem.c: InitHeap has allocated space during a previous call\n");
        return -1;
    }
    if (sizeOfRegion <= 0) {
        fprintf(stderr, "Error:mem.c: Requested block size is not positive\n");
        return -1;
    }

    // Get the pagesize
    pagesize = getpagesize();

    // Calculate padsize as the padding required to round up sizeOfRegion 
    // to a multiple of pagesize
    padsize = sizeOfRegion % pagesize;
    padsize = (pagesize - padsize) % pagesize;

    allocsize = sizeOfRegion + padsize;

    // Using mmap to allocate memory
    fd = open("/dev/zero", O_RDWR);
    if (-1 == fd) {
        fprintf(stderr, "Error:mem.c: Cannot open /dev/zero\n");
        return -1;
    }
    mmap_ptr = mmap(NULL, allocsize, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (MAP_FAILED == mmap_ptr) {
        fprintf(stderr, "Error:mem.c: mmap cannot allocate space\n");
        allocated_once = 0;
        return -1;
    }
  
    allocated_once = 1;

    // for double word alignment and end mark
    allocsize -= 8;

    // Initially there is only one big free block in the heap.
    // Skip first 4 bytes for double word alignment requirement.
    heapStart = (blockHeader*) mmap_ptr + 1;

    // Set the end mark
    endMark = (blockHeader*)((void*)heapStart + allocsize);
    endMark->size_status = 1;

    // Set size in header
    heapStart->size_status = allocsize;

    // Set p-bit as allocated in header
    // note a-bit left at 0 for free
    heapStart->size_status += 2;

    // Set the footer
    blockHeader *footer = (blockHeader*) ((void*)heapStart + allocsize - 4);
    footer->size_status = allocsize;
  
    return 0;
} 
                  
/* 
 * Function to be used for DEBUGGING to help you visualize your heap structure.
 * Prints out a list of all the blocks including this information:
 * No.      : serial number of the block 
 * Status   : free/used (allocated)
 * Prev     : status of previous block free/used (allocated)
 * t_Begin  : address of the first byte in the block (where the header starts) 
 * t_End    : address of the last byte in the block 
 * t_Size   : size of the block as stored in the block header
 */                     
void dumpMem() {     
 
    int counter;
    char status[5];
    char p_status[5];
    char *t_begin = NULL;
    char *t_end   = NULL;
    int t_size;

    blockHeader *current = heapStart;
    counter = 1;

    int used_size = 0;
    int free_size = 0;
    int is_used   = -1;

    fprintf(stdout, "************************************Block list***\
                    ********************************\n");
    fprintf(stdout, "No.\tStatus\tPrev\tt_Begin\t\tt_End\t\tt_Size\tfooter_val\n");
    fprintf(stdout, "-------------------------------------------------\
                    --------------------------------\n");
  
    while (current->size_status != 1) {
        t_begin = (char*)current;
        t_size = current->size_status;
        int free = 1;
        int footer_val = 0;
        if (t_size & 1) {
            // LSB = 1 => used block
            strcpy(status, "used");
            is_used = 1;
            t_size = t_size - 1;
        } else {
            strcpy(status, "Free");
            is_used = 0;
        }

        if (t_size & 2) {
            strcpy(p_status, "used");
            t_size = t_size - 2;
        } else {
            strcpy(p_status, "Free");
        }

        if(free==1) {
            blockHeader *footer = current + (t_size/4 - 1);
            footer_val = footer->size_status;
        }

        if (is_used) 
            used_size += t_size;
        else 
            free_size += t_size;

        t_end = t_begin + t_size - 1;
    
        fprintf(stdout, "%d\t%s\t%s\t0x%08lx\t0x%08lx\t%d\t%d\n", counter, status,
        p_status, (unsigned long int)t_begin, (unsigned long int)t_end, t_size, footer_val);
    
        current = (blockHeader*)((char*)current + t_size);
        counter = counter + 1;
    }

    fprintf(stdout, "---------------------------------------------------\
                    ------------------------------\n");
    fprintf(stdout, "***************************************************\
                    ******************************\n");
    fprintf(stdout, "Total used size = %d\n", used_size);
    fprintf(stdout, "Total free size = %d\n", free_size);
    fprintf(stdout, "Total size = %d\n", used_size + free_size);
    fprintf(stdout, "***************************************************\
                    ******************************\n");
    fflush(stdout);

    return;  
} 
