
#include <cstdint>
#include <cstdio>

// todo: needed/allowed?
#include <cstdlib>
#include <stdbool.h>

extern "C" {


// This macro allows us to prefix strings so that they are less likely to
// conflict with existing symbol names in the examined programs.
// e.g. TOLERATE(entry) yields ToLeRaToR_entry
#define TOLERATE(X) ToLeRaToR_##X

// todo: size?  make proper dynamic data structure
#define MAX_NUM_TRACKED_ALLOCATIONS 1024

struct Allocation {
  int8_t* address;
  int64_t size;
};

struct Allocation nullAllocation = {0, 0};

int heapAllocationsIndex;
struct Allocation heapAllocations[MAX_NUM_TRACKED_ALLOCATIONS];

int stackAllocationsIndex;
struct Allocation stackAllocations[MAX_NUM_TRACKED_ALLOCATIONS];


// todo: init using sumner method instead
void
TOLERATE(initializeTracker)() {
  heapAllocationsIndex = 0;
  stackAllocationsIndex = 0;

  // struct Allocation tempAlloc = {0, 0};
  for (int i = 0; i < MAX_NUM_TRACKED_ALLOCATIONS; ++i) {
    heapAllocations[i] = nullAllocation;
    stackAllocations[i] = nullAllocation;
  }
  // printf("Tracking initialized\n");
}

/* TRACK ALLOCATIONS */
void
TOLERATE(registerMalloc)(int8_t* address, int64_t size) {
// TOLERATE(registerMalloc)(int64_t size) {
  // printf("Registering Malloc: size = %d\n", size);
  // printf("Registering Malloc: address = %p, size = %lld\n", address, (long long)size);

  // struct Allocation allocation = {address, size};
  struct Allocation allocation = {address, size};
  // todo: check not within existing allocated space?
  heapAllocations[heapAllocationsIndex] = allocation;
  heapAllocationsIndex++;

  // todo: better tracking?  dynamic data structure removes need
  if (heapAllocationsIndex >= MAX_NUM_TRACKED_ALLOCATIONS) {
    fprintf(stderr, "ERROR: max tracked heap allocations exceeded\n");
    exit(-1);
  }
}

void
TOLERATE(registerAlloca)(int8_t* address, int64_t size) {
  // printf("Registering Alloca: address = %p, size = %lld\n", address, (long long)size);

  struct Allocation allocation = {address, size};
  // todo: check not within existing allocated space?
  stackAllocations[stackAllocationsIndex] = allocation;
  stackAllocationsIndex++;

  // todo: better tracking?  dynamic data structure removes need
  if (stackAllocationsIndex >= MAX_NUM_TRACKED_ALLOCATIONS) {
    fprintf(stderr, "ERROR: max tracked stack allocations exceeded\n");
    exit(-1);
  }
} 

void
TOLERATE(unregisterAlloca)(int8_t* address) {
  // printf("Unregistering Alloca: address = %p\n", address);

  for (int i = 0; i < stackAllocationsIndex; ++i) {
    if (stackAllocations[i].address == address) {
      stackAllocations[i] = nullAllocation;
    }
  }

  // printf("Error: could not unregister: %p\n", address);
}

/* TRACK THREADING FUNCTIONS */
// todo: takes an argument of some kind?
void
TOLERATE(onPthreadCreate)() {
  // todo: call a function here to determine tid and create new vector clock
  // update existing vector clocks
  // update shadow memory (todo: stretch?)
  fprintf(stderr, "Injected pthread_create\n");
}

void
TOLERATE(onPthreadJoin)() {
  // todo: call a function here to determine tid and create new vector clock
  // update existing vector clocks
  // update shadow memory (todo: stretch?)
  fprintf(stderr, "Injected pthread_join\n");
}

void
TOLERATE(onMutexLock)() {
  // todo: call a function here to determine tid, update vector clock
  // update existing vector clocks
  // update shadow memory (todo: stretch?)
  fprintf(stderr, "Injected mutexLock\n");
}

void
TOLERATE(onMutexUnlock)() {
  // todo: call a function here to determine tid, update vector clock
  // update existing vector clocks
  // update shadow memory (todo: stretch?)
  fprintf(stderr, "Injected mutexUnlock\n");
}



/* LOAD */
void
// TOLERATE(isValidLoadWithExit)(int8_t* address, int64_t size) { // todo: size needed?
TOLERATE(isValidLoadWithExit)(int8_t* address) { // todo: size needed?
  // todo: temp
  int64_t size = 0; 
  // printf("Load from: address = %p\n", address);

  int8_t* memoryAccessStart = address;
  int8_t* memoryAccessEnd = address + size;

  int8_t* validMemoryStart;
  int8_t* validMemoryEnd;

  for (int i = 0; i < heapAllocationsIndex; ++i) {
    if (heapAllocations[i].address == 0) {
      continue;
    }

    validMemoryStart = heapAllocations[i].address;
    validMemoryEnd = heapAllocations[i].address + heapAllocations[i].size;

    // todo: <= end instead?
    if (memoryAccessStart >= validMemoryStart && memoryAccessEnd < validMemoryEnd) { 
      // printf("Valid heap access: address: %p + size: %lld == computed: %p\n", address, (long long)size, address+size);
      return;
    }
  }

  // todo: <= end instead?
  for (int i = 0; i < stackAllocationsIndex; ++i) {
    if (stackAllocations[i].address == 0) {
      continue;
    }

    validMemoryStart = stackAllocations[i].address;
    validMemoryEnd = stackAllocations[i].address + stackAllocations[i].size;

    if (memoryAccessStart >= validMemoryStart && memoryAccessEnd < validMemoryEnd) {
      // printf("Valid stack access: address: %p + size: %lld == computed: %p\n", address, (long long)size, address+size);
      return;
    }
  }

  fprintf(stderr, "FOUND: Invalid read from memory\n");
}




/* STORE */
void
// TOLERATE(isValidStoreWithExit)(int8_t* address, int64_t size) { // todo: size needed?
TOLERATE(isValidStoreWithExit)(int8_t* address) { // todo: size needed?
  // todo: temp
  int64_t size = 0; 
  // printf("Store to: address = %p\n", address);

  int8_t* memoryAccessStart = address;
  int8_t* memoryAccessEnd = address + size;

  int8_t* validMemoryStart;
  int8_t* validMemoryEnd;

  for (int i = 0; i < heapAllocationsIndex; ++i) {
    if (heapAllocations[i].address == 0) {
      continue;
    }

    validMemoryStart = heapAllocations[i].address;
    validMemoryEnd = heapAllocations[i].address + heapAllocations[i].size;

    // todo: <= end instead?
    if (memoryAccessStart >= validMemoryStart && memoryAccessEnd < validMemoryEnd) { 
      // printf("Valid heap access: address: %p + size: %lld == computed: %p\n", address, (long long)size, address+size);
      return;
    }
  }

  // todo: <= end instead?
  for (int i = 0; i < stackAllocationsIndex; ++i) {
    if (stackAllocations[i].address == 0) {
      continue;
    }

    validMemoryStart = stackAllocations[i].address;
    validMemoryEnd = stackAllocations[i].address + stackAllocations[i].size;

    if (memoryAccessStart >= validMemoryStart && memoryAccessEnd < validMemoryEnd) {
      // printf("Valid stack access: address: %p + size: %lld == computed: %p\n", address, (long long)size, address+size);
      return;
    }
  }

  fprintf(stderr, "FOUND: Invalid write to memory\n");
}




/* FREE */
void
TOLERATE(isValidFreeWithExit)(int8_t* address) {
  struct Allocation tempAlloc = {0, 0};

  for (int i = 0; i < MAX_NUM_TRACKED_ALLOCATIONS; ++i) {
    // todo: improve with hash map.  Also, need to check not 0,0?
    if (heapAllocations[i].address == address) {
      heapAllocations[i] = tempAlloc;
      // printf("TEST: successful free\n");
      return;
    }
  }

  // free address not found as allocated, double/invalid free
  fprintf(stderr, "FOUND: Invalid free of memory\n");
}







/* TESTING */

void
TOLERATE(helloworld)() {
  printf("==============================\n"
         "\tHello, Hello, World!\n"
         "==============================\n");
}

// todo: testing only, remove
void
TOLERATE(goodbyeworld)() {
  printf("\nTracked heap allocations remaining at exit:\n");
  for (int i = 0; i < MAX_NUM_TRACKED_ALLOCATIONS; ++i) {
    if (heapAllocations[i].address != 0) {
      printf("--> address = %p, size = %lld\n", heapAllocations[i].address, (long long)heapAllocations[i].size);
    }
  }

  printf("\nTracked stack allocations remaining at exit:\n");
  for (int i = 0; i < MAX_NUM_TRACKED_ALLOCATIONS; ++i) {
    if (stackAllocations[i].address != 0) {
      printf("--> address = %p, size = %lld\n", stackAllocations[i].address, (long long)stackAllocations[i].size);
    }
  }
}


} // extern 'C'