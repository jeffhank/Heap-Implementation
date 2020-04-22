

#ifndef __heapAlloc_h
#define __heapAlloc_h

int   initHeap (int sizeOfRegion);
void* allocHeap(int size);
int   freeHeap (void *ptr);
void  dumpMem  ();

void* malloc(size_t size) {
    return NULL;
}

#endif // __heapAlloc_h__
