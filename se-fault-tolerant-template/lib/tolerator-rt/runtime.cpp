#include <vector>
#include <unordered_map>
#include <iostream>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdbool.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <mutex>



extern "C" {

// This macro allows us to prefix strings so that they are less likely to
// conflict with existing symbol names in the examined programs.
// e.g. TOLERATE(entry) yields ToLeRaToR_entry
#define TOLERATE(X) ToLeRaToR_##X

// todo: size?  make proper dynamic data structure
#define MAX_NUM_TRACKED_ALLOCATIONS 1024


/* BEGIN Previous tracking structures */
// todo: improve using STL
struct Allocation {
  int8_t* address;
  int64_t size;
};

struct Allocation nullAllocation = {0, 0};

int heapAllocationsIndex;
struct Allocation heapAllocations[MAX_NUM_TRACKED_ALLOCATIONS];

int stackAllocationsIndex;
struct Allocation stackAllocations[MAX_NUM_TRACKED_ALLOCATIONS];
/* END Previous tracking structures */


/* Vector Clocks*/
typedef std::vector<uint64_t> VectorClock;


// todo: initialize clocks to zero when a thread is spawned
class ShadowMemory {
private:
  struct ReadWriteClockPair {
    VectorClock readClock;
    VectorClock writeClock;
  };

  std::unordered_map<uintptr_t, ReadWriteClockPair> memoryMap;

public:
  void readAccess(uintptr_t address, size_t clockIndex) {
    printf("shadow memory read access\n");

    // todo: this can be more efficient
    if (memoryMap.find(address) == memoryMap.end()) {
      // not found, create a new vector clock
      memoryMap[address] = ReadWriteClockPair();
    }

    // update the read clock for the given index
    memoryMap[address].readClock[clockIndex]++;
  }

  // todo: need a separate function, or can be same?
  void writeAccess(uintptr_t address, size_t clockIndex) {
    printf("shadow memory write access\n");

    // todo: this can be more efficient
    if (memoryMap.find(address) == memoryMap.end()) {
      // not found, create a new vector clock
      memoryMap[address] = ReadWriteClockPair();
    }

    // update the read clock for the given index
    memoryMap[address].writeClock[clockIndex]++;
  }

  void printClocks(uintptr_t address) {
    if (memoryMap.find(address) != memoryMap.end()) {
      std::cout << "Read Clock: ";
      for (auto time : memoryMap[address].readClock) {
        std::cout << time << " ";
      }
      std::cout << "Write Clock: ";
      for (auto time : memoryMap[address].writeClock) {
        std::cout << time << " ";
      }
      std::cout << std::endl;
    } else {
      std::cout << "Error: address not found in shadow memory" << std::endl;
    }
  }
};

// VECTOR CLOCKS
std::vector<VectorClock> threadClocks;
ShadowMemory shadowMemory;
std::vector<VectorClock> lockClocks;

std::mutex threadClocks_mutex;
std::mutex shadowMemory_mutex;
std::mutex lockClocks_mutex;

// todo: map of thread ids to vector clock indexes.  whenever we create a thread, add a new index to every vector clock = 0

/* HELPER FUNCTIONS */
void receiveProgress(long tid, VectorClock mutexClock) {
  // TODO
}

void advanceLocal(long tid) {
  // TODO
}


// todo: init using sumner method instead
void
TOLERATE(initializeTracker)() {
  // ShadowMemory shadowMemory;

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
// todo: also track initial (main) thread

// todo: takes an argument of some kind?
void
TOLERATE(onPthreadCreate)() {
  // todo: call a function here to determine tid and create new vector clock
  // update existing vector clocks
  // update shadow memory (todo: stretch?)
  // int tid = pthread_self();

  // todo: use first argument of function call in LLVM to automatically update 

  // long tid = syscall(__NR_gettid);
  // fprintf(stdout, "pthread_create, tid: %d\n", tid);
  fprintf(stdout, "pthread_create\n");
}

void
TOLERATE(onPthreadJoin)() {
  // update existing vector clocks - remove?
  // update shadow memory (todo: stretch?)

  fprintf(stdout, "pthread_join\n");
}

void
TOLERATE(onMutexLock)() {
  // todo: call a function here to determine tid, update vector clock
  // update existing vector clocks
  // update shadow memory (todo: stretch?)
  long tid = syscall(__NR_gettid);
  fprintf(stdout, "mutex_lock, tid: %d\n", tid);
  // fprintf(stderr, "Injected mutexLock\n");
}

void
TOLERATE(onMutexUnlock)() {
  // todo: call a function here to determine tid, update vector clock
  // update existing vector clocks
  // update shadow memory (todo: stretch?)
  // fprintf(stderr, "Injected mutexUnlock\n");
  long tid = syscall(__NR_gettid);
  fprintf(stdout, "mutex_unlock, tid: %d\n", tid);
}



/* LOAD */
void
// TOLERATE(isValidLoadWithExit)(int8_t* address, int64_t size) { // todo: size needed?
TOLERATE(isValidLoadWithExit)(int8_t* address) { // todo: size needed?
  // check against last write

  fprintf(stdout, "LOAD (Read)\n");
}




/* STORE */
void
// TOLERATE(isValidStoreWithExit)(int8_t* address, int64_t size) { // todo: size needed?
TOLERATE(isValidStoreWithExit)(int8_t* address) { // todo: size needed?
  // check against last read and write

  fprintf(stdout, "STORE (Write)\n");
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