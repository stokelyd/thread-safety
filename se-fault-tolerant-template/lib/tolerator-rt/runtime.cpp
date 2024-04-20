#include <vector>
#include <unordered_map>
#include <iostream>
#include <algorithm>

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
// typedef std::vector<uint64_t> VectorClock;

class VectorClock {
public:
  std::unordered_map<long, uint64_t> vectorClock;
  std::mutex vectorClock_mutex;

public:
  VectorClock() {}

  ~VectorClock() {}

  // copy ctor
  VectorClock(const VectorClock& other) : vectorClock(other.vectorClock) {}

  void advanceLocal(long tid) {
    // todo: bounds check/init to 0?
    std::lock_guard<std::mutex> lock(vectorClock_mutex);
    vectorClock[tid]++;
  }

  // updates a lock with the progress of the current thread
  void sendProgress(VectorClock& lock) {
    // todo - correct semantics?
    lock.merge(*this);
  }

  // receives the progress from a given lock
  void receiveProgress(VectorClock& lock) {
    // todo - correct semantics?
    merge(lock);
  }

  // merge another VectorClock into this one, taking the max of each clock value
  void merge(const VectorClock& other) {
    for (const auto& entry : other.vectorClock) {
      vectorClock[entry.first] = std::max(vectorClock[entry.first], entry.second);
    }
  }

  // return true if this VectorClock is less than or equal to the other (elementwise)
  bool happensBefore(const VectorClock& other) {
    bool isLessOrEqual = true;
    for (const auto& entry : vectorClock) {
      auto it = other.vectorClock.find(entry.first);
      if (it != other.vectorClock.end()) {
        // same entry found
        if (entry.second > it->second) {
          isLessOrEqual = false;
          break;
        }
      } else {
        // entry not found
        isLessOrEqual = false;
        break;
      }
    }
    return isLessOrEqual;
  }

  void print() {
    for (const auto& entry : vectorClock) {
      std::cout << "Clock " << entry.first << ": " << entry.second << "\n";
      // printf("Clock %ld: %d", entry.first, entry.second);
    }
  }
};


class VectorClockManager {
public:
  std::unordered_map<long, VectorClock> clocks;
  std::mutex threadClock_mutex;

public:
  // void addVectorClock(long id) {
  //   clocks.emplace(id, VectorClock());
  // }

  VectorClock& getVectorClock(long id) {
    std::lock_guard<std::mutex> lock(threadClock_mutex); // todo: verify
    return clocks[id];
  }

  // bool contains(long id) {
  //   return clocks.find(id) == clocks.end() ? false : true;
  // }

  // void registerNewClockIfNotContains(long id) {
  //   if (!contains(id)) {
  //     addVectorClock(id);
  //   }
  // }

  void printAllClocks() {
    for (auto& clock : clocks) {
      clock.second.print();
    }
  }
};






// class LockManager {
// private:
//   std::unordered_map<
// }


// typedef std::vector<uint64_t> ShadowMemoryVectorClock;
// todo: need to initialize clocks to zero when a thread is spawned?
// todo: use struct instead.  for now, Read = first, write = second
class ShadowMemory {
public:
  struct ReadWriteClockPair {
    VectorClock readClock;
    VectorClock writeClock;

    // custom ctor
    // ReadWriteClockPair() : readClock(), writeClock() {}
  };

  // std::unordered_map<uintptr_t, ReadWriteClockPair> memoryMap;
  std::unordered_map<uintptr_t, std::pair<VectorClock, VectorClock>> memoryMap;
  std::mutex shadowMemory_mutex;

  // VectorClock readClocks;
  // VectorClock writeClocks;
  // std::mutex readClocks_mutex;
  // std::mutex writeClocks_mutex;

public:
  ShadowMemory() {}

  void addMemoryLocation(uintptr_t address) {
    if (memoryMap.find(address) == memoryMap.end()) {
      // memoryMap.emplace(address, std::pair<VectorClock, VectorClock>(VectorClock(), VectorClock()));
      memoryMap.emplace(address, std::make_pair(VectorClock(), VectorClock()));
    }
  }

  void readAccess(uintptr_t address, const VectorClock& readThreadClock) {
    auto it = memoryMap.find(address);
    if (it != memoryMap.end()) {
      // update readClock
      it->second.first.merge(readThreadClock);
    } else {
      // address not yet tracked, add with provided clock
      memoryMap.emplace(address, std::make_pair(readThreadClock, VectorClock()));
    }
  }

  void writeAccess(uintptr_t address, const VectorClock& writeThreadClock) {
    auto it = memoryMap.find(address);
    if (it != memoryMap.end()) {
      // update readClock
      it->second.second.merge(writeThreadClock);
    } else {
      // address not yet tracked, add with provided clock
      memoryMap.emplace(address, std::make_pair(writeThreadClock, VectorClock()));
    }
  }

  void print() {
    for (auto& location: memoryMap) {
      printf("\nLOCATION:\n");
      // std::cout << "Read: " << location.second.first. << " Write: " << location.second.second.print() << "\n";
      printf("Read: ");
      location.second.first.print();

      printf("Write:");
      location.second.second.print();
    }
  }
};

// VECTOR CLOCKS
// std::vector<VectorClock> threadClocks;
// ShadowMemory shadowMemory;
// std::vector<VectorClock> lockClocks;

// std::mutex threadClocks_mutex;
// std::mutex shadowMemory_mutex;
// std::mutex lockClocks_mutex;


VectorClockManager* threads = nullptr;
VectorClockManager* locks = nullptr;
ShadowMemory* shadowMemory = nullptr;



/* HELPER FUNCTIONS */
// void sendProgress(long tid, VectorClock mutexClock) {
//   auto it = std::find(threadIds.begin(), threadIds.end(), tid);

//   if (it != threadIds.end()) {
//     int index = it - threadIds.begin();
//   }
  
// }

// void advanceLocal(long tid) {
//   // TODO
// }


// todo: map of thread ids to vector clock indexes.  whenever we create a thread, add a new index to every vector clock = 0
std::vector<long> threadIds;
std::mutex threadIds_mutex;

long getCurrentTid() {
  long tid = syscall(__NR_gettid);

  // todo: this is temporary, create a more robust solution - still needed at all?
  threadIds_mutex.lock();
  {
    if(std::find(threadIds.begin(), threadIds.end(), tid) != threadIds.end()) {
      // threadId is currently tracked
    } else {
      threadIds.push_back(tid);
      // printf("Adding tid: %ld\n", tid);
      // update all existing clocks?
    }
  }
  threadIds_mutex.unlock();

  return tid;
}



// todo: init using sumner method instead
void
TOLERATE(initializeTracker)() {
  // ShadowMemory shadowMemory;

  long tid = getCurrentTid();
  printf("initialize: tid=%d\n", tid);

  // todo: use unique_ptr instead of new
  locks = new VectorClockManager();
  threads = new VectorClockManager();
  shadowMemory = new ShadowMemory();



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
  // fprintf(stdout, "pthread_create\n");
}

void
TOLERATE(onPthreadJoin)() {
  // update existing vector clocks - remove
  // update shadow memory (todo: stretch?)

  // fprintf(stdout, "pthread_join\n");
}

void
TOLERATE(onMutexLock)(int8_t* mutex) {
  // todo: call a function here to determine tid, update vector clock
  // update existing vector clocks
  // update shadow memory (todo: stretch?)
  long tid = getCurrentTid();
  printf("mutex_lock: tid=%d mutex=%hd\n", tid, mutex);


  VectorClock& threadClock = threads->getVectorClock(tid);
  VectorClock& mutexClock = locks->getVectorClock(tid);
  
  threadClock.receiveProgress(mutexClock);
  threadClock.advanceLocal(tid);
  // printf("After advance: ");
  // mutexClock.print();

  // printf("All: ");
  // locks->printAllClocks();
}  

void
TOLERATE(onMutexUnlock)(int8_t* mutex) {
  // todo: call a function here to determine tid, update vector clock
  // update existing vector clocks
  // update shadow memory (todo: stretch?)

  long tid = getCurrentTid();
  printf("mutex_unlock: tid=%d mutex=%hd\n", tid, mutex);

  VectorClock& threadClock = threads->getVectorClock(tid);
  VectorClock& mutexClock = locks->getVectorClock(tid);
  
  threadClock.sendProgress(mutexClock);
  threadClock.advanceLocal(tid);
}



/* LOAD */
void
// TOLERATE(isValidLoadWithExit)(int8_t* address, int64_t size) { // todo: size needed?
TOLERATE(isValidLoadWithExit)(int8_t* address) { // todo: size needed?
  
  long tid = getCurrentTid();

  // printf("LOAD\n");
  uintptr_t address_actual = (uintptr_t)address;

  // check against last write
  VectorClock& lastWrite = shadowMemory->memoryMap[address_actual].second;
  VectorClock& threadClock = threads->getVectorClock(tid);

  if (lastWrite.happensBefore(threadClock)) {
    // printf("valid load\n");
  } else {
    printf("INVALID LOAD\n");
  }

  shadowMemory->readAccess(address_actual, threads->getVectorClock(tid));
}




/* STORE */
void
// TOLERATE(isValidStoreWithExit)(int8_t* address, int64_t size) { // todo: size needed?
TOLERATE(isValidStoreWithExit)(int8_t* address) { // todo: size needed?
  // check against last read and write

  long tid = getCurrentTid();

  // printf("STORE\n");
  uintptr_t address_actual = (uintptr_t)address;

  // check against last read and write
  VectorClock& lastRead = shadowMemory->memoryMap[address_actual].first;
  VectorClock& lastWrite = shadowMemory->memoryMap[address_actual].second;
  VectorClock& threadClock = threads->getVectorClock(tid);

  if (lastWrite.happensBefore(threadClock) && lastRead.happensBefore(threadClock)) {
  } else {
    printf("INVALID STORE\n");
  }

  shadowMemory->writeAccess(address_actual, threads->getVectorClock(tid));
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



void
TOLERATE(registerIfNewThread)() {
  long tid = getCurrentTid();
  fprintf(stdout, "tid: %d\n", tid);

  // todo
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
  printf("\nLock Clock Status at exit:\n");
  locks->printAllClocks();
  free(locks);

  printf("\nThread Clock Status at exit:\n");
  threads->printAllClocks();
  free(threads);

  printf("\nShadow Memory Status at exit:\n");
  shadowMemory->print();
  free(shadowMemory);
}


} // extern 'C'