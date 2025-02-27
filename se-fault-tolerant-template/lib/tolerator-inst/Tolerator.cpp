

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"

#include "llvm/IR/InstVisitor.h"

#include "llvm/IR/Constants.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/IR/AbstractCallSite.h"
#include "llvm/IR/NoFolder.h"

#include "Tolerator.h"
#include <unordered_set>


using namespace llvm;
using tolerator::Tolerator;

namespace tolerator {

char Tolerator::ID = 0;

}

// EXTERNAL FUNCTIONS
FunctionCallee initializeTracker;
FunctionCallee registerMalloc;
FunctionCallee registerAlloca;
FunctionCallee unregisterAlloca;

FunctionCallee isValidLoadWithExit;
FunctionCallee isValidStoreWithExit;
FunctionCallee isValidFreeWithExit;

FunctionCallee onPthreadCreate;
FunctionCallee onPthreadJoin;
FunctionCallee onMutexLock;
FunctionCallee onMutexUnlock;

FunctionCallee registerIfNewThread;

// TYPES
Type* voidTy;
IntegerType* int1Ty; 
IntegerType* int32Ty; 

IntegerType* int64Ty;
PointerType* int8PtrTy; 
PointerType* int32PtrTy;


// todo: store as explicitly casted instructions
struct InstrumentationVisitor : public InstVisitor<InstrumentationVisitor> {
  std::vector<Instruction*> mallocInstructions;
  std::vector<Instruction*> freeInstructions;
  std::vector<Instruction*> allocaInstructions;
  std::vector<Instruction*> loadInstructions;
  std::vector<Instruction*> storeInstructions;

  std::vector<Instruction*> pthreadCreateInstructions;
  std::vector<Instruction*> pthreadJoinInstructions;
  std::vector<Instruction*> pthreadMutexLockInstructions;
  std::vector<Instruction*> pthreadMutexUnlockInstructions;

  void visitAllocaInst(AllocaInst &I) {
    // outs() << "Visisting alloca...\n";
    // cast<AllocaInst>(I);
    allocaInstructions.push_back(&I);
  }

  void visitLoadInst(LoadInst &I) {
    // outs() << "Visiting load...\n";
    loadInstructions.push_back(&I);
  }

  void visitStoreInst(StoreInst &I) {
    // outs() << "Visiting store...\n";
    storeInstructions.push_back(&I);
  }

  /* Function calls */
  void visitCallInst(CallInst &I) {
    if (I.getCalledFunction()->getName() == "malloc") {
      // outs() << "Visiting malloc...\n";
      mallocInstructions.push_back(&I);
    } 
    
    if (I.getCalledFunction()->getName() == "free") {
      // outs() << "Visiting free...\n";
      freeInstructions.push_back(&I);
    } 

    if (I.getCalledFunction()->getName() == "pthread_create") {
      outs() << "Visiting pthread_create...\n";
      pthreadCreateInstructions.push_back(&I);
    } 

    if (I.getCalledFunction()->getName() == "pthread_join") {
      outs() << "Visiting pthread_join...\n";
      pthreadJoinInstructions.push_back(&I);
    } 

    if (I.getCalledFunction()->getName() == "pthread_mutex_lock") {
      outs() << "Visiting pthread_mutex_lock...\n";
      pthreadMutexLockInstructions.push_back(&I);
    } 

    if (I.getCalledFunction()->getName() == "pthread_mutex_unlock") {
      outs() << "Visiting pthread_mutex_unlock...\n";
      pthreadMutexUnlockInstructions.push_back(&I);
    } 
  }

};






void 
instrumentMallocInstruction(Instruction* I) {
  // insert instrumentation AFTER malloc instruction, to get address value
  IRBuilder<> Builder(I->getNextNode());

  Value* address = cast<Value>(I);
  Value* size = I->getOperand(0);
  Value* args[] = { address, size };

  // todo: ensure no validity check needed for malloc ops
  Builder.CreateCall(llvm::cast<Function>(registerMalloc.getCallee()), args);
}


void
instrumentAllocaInstruction(Instruction* I) {
  IRBuilder<> Builder(I->getNextNode());

  DataLayout* DL = new DataLayout(I->getModule());
  auto* AI = cast<AllocaInst>(I);

  Type* allocatedType = AI->getAllocatedType();
  int allocationSize; // todo: type?  i64?
  if (ArrayType* array = dyn_cast<ArrayType>(allocatedType)) { 
    // local allocation of an array  
    int arraySize = array->getNumElements();
    TypeSize typeSize = DL->getTypeAllocSize(array->getArrayElementType());
    // outs() << "Array of size: " << arraySize << "\n";
    // outs() << "Element size: " << typeSize << "\n";
    allocationSize = arraySize * typeSize;
    // outs() << "Array allocation of size: " << allocationSize << "\n";
  } else {
    allocationSize = DL->getTypeAllocSize(allocatedType);
    // outs() << "Single element of size: " << allocationSize << "\n";
  }

  Value* size = Builder.getInt64(allocationSize);

  // calculate address (todo: verify)
  auto* bitcast = new BitCastInst(cast<Value>(AI), int32PtrTy, "allocInst", I->getNextNode());
  Value* address = cast<Value>(bitcast);
  // outs() << "address: " << address << "\n\n";

  Value* args[] = { address, size };

  Builder.CreateCall(llvm::cast<Function>(registerAlloca.getCallee()), args);

  auto* parentFunc = I->getFunction();

  // todo: correctly tracks when local allocations go out of scope?  use basic blocks instead?
  std::vector<Instruction*> terminators;
  for (auto &BB : *parentFunc) {
    for (auto &I : BB) {
      if (auto* RI = dyn_cast<ReturnInst>(&I)) {
        terminators.push_back(&I);
      }
    }
  }

  for (auto* terminatorInst : terminators) {
    Builder.SetInsertPoint(terminatorInst);
    Value* args_deallocate[] = { address };
    Builder.CreateCall(llvm::cast<Function>(unregisterAlloca.getCallee()), args_deallocate);
  }

}


void
Tolerator::instrumentLoadInstruction(Instruction* I) {
  tolerator::AnalysisType analysisType = tolerator::Tolerator::ANALYSIS_TYPE;
  auto* LI = cast<LoadInst>(I);

  IRBuilder<> Builder(I);
  // DataLayout* DL = new DataLayout(I->getModule());

  // BitCastInst* bitcast = new BitCastInst(LI->getPointerOperand(), int32PtrTy, "loadInst", I->getNextNode());
  BitCastInst* bitcast = new BitCastInst(LI->getPointerOperand(), int32PtrTy, "loadInst", I);
  Value* address = cast<Value>(bitcast);

  // // // PointerType* PT = cast<PointerType>(LI->getPointerOperand()->getType());
  // // Type* PT = LI->getPointerOperandType();
  
  // // uint64_t tempSize = DL->getTypeStoreSize(PT->getNonOpaquePointerElementType()); // todo: opaque?
  
  // // // Value* size = ConstantInt::get(int64Ty, size); // todo: use IRBuilder instead
  // // Value* size = Builder.getInt64(tempSize);

  // // // Value* args[] = { address, size };
  Value* args[] = { address };

  
  // outs() << "LOAD: lOGGING\n";
  Builder.CreateCall(llvm::cast<Function>(isValidLoadWithExit.getCallee()), args); 
}


void
Tolerator::instrumentStoreInstruction(Instruction* I) {
  tolerator::AnalysisType analysisType = tolerator::Tolerator::ANALYSIS_TYPE;
  auto* SI = cast<StoreInst>(I);

  IRBuilder<> Builder(I);
  // DataLayout* DL = new DataLayout(I->getModule());

  // BitCastInst* bitcast = new BitCastInst(LI->getPointerOperand(), int32PtrTy, "loadInst", I->getNextNode());
  BitCastInst* bitcast = new BitCastInst(SI->getPointerOperand(), int32PtrTy, "storeInst", I);
  Value* address = cast<Value>(bitcast);

  // // // PointerType* PT = cast<PointerType>(LI->getPointerOperand()->getType());
  // // Type* PT = LI->getPointerOperandType();
  
  // // uint64_t tempSize = DL->getTypeStoreSize(PT->getNonOpaquePointerElementType()); // todo: opaque?

  // // // Value* size = ConstantInt::get(int64Ty, size); // todo: use IRBuilder instead
  // // Value* size = Builder.getInt64(tempSize);

  // // // Value* args[] = { address, size };
  Value* args[] = { address };

  // outs() << "STORE: lOGGING\n";
  Builder.CreateCall(llvm::cast<Function>(isValidStoreWithExit.getCallee()), args);
}


void 
Tolerator::instrumentFreeInstruction(Instruction* I) {
  tolerator::AnalysisType analysisType = tolerator::Tolerator::ANALYSIS_TYPE;

  IRBuilder<> Builder(I);
  Value* address = I->getOperand(0);
  Value* args[] = { address };

  // outs() << "FREE: lOGGING\n";
  Builder.CreateCall(llvm::cast<Function>(isValidFreeWithExit.getCallee()), args);
}

/* Thread Functions */
void 
Tolerator::instrumentPthreadCreateInstruction(Instruction* I) {
  tolerator::AnalysisType analysisType = tolerator::Tolerator::ANALYSIS_TYPE;

  IRBuilder<> Builder(I);
  Value* address = I->getOperand(0);
  Value* args[] = { address };

  outs() << "pthread_create: lOGGING\n";

  DILocation* debugLoc = I->getDebugLoc();
  if (debugLoc) {
    outs() << "Function call at " << debugLoc->getLine() << "\n";    
    outs() << "Filename: " << I->getFunction()->getParent()->getSourceFileName() << "\n";
  }

  Builder.CreateCall(llvm::cast<Function>(onPthreadCreate.getCallee()));
}

void 
Tolerator::instrumentPthreadJoinInstruction(Instruction* I) {
  tolerator::AnalysisType analysisType = tolerator::Tolerator::ANALYSIS_TYPE;

  IRBuilder<> Builder(I);
  Value* address = I->getOperand(0);
  Value* args[] = { address };

  // outs() << "pthread_join: lOGGING\n";
  Builder.CreateCall(llvm::cast<Function>(onPthreadJoin.getCallee()));
}

void 
Tolerator::instrumentPthreadMutexLockInstruction(Instruction* I) {
  tolerator::AnalysisType analysisType = tolerator::Tolerator::ANALYSIS_TYPE;

  IRBuilder<> Builder(I);
  Value* mutex = I->getOperand(0);
  Value* args[] = { mutex };

  // outs() << "pthread_mutex_lock: lOGGING\n";
  Builder.CreateCall(llvm::cast<Function>(onMutexLock.getCallee()), args);
}

void 
Tolerator::instrumentPthreadMutexUnlockInstruction(Instruction* I) {
  tolerator::AnalysisType analysisType = tolerator::Tolerator::ANALYSIS_TYPE;

  IRBuilder<> Builder(I);
  Value* mutex = I->getOperand(0);

  // auto* bitcast = new BitCastInst(cast<Value>(I), int32PtrTy, "mutexUnlockInst", I->getNextNode());
  // Value* address = cast<Value>(bitcast);
  // outs() << "Address: " << address << "\n";
  
  // todo: better location?
  // CallInst* ci = cast<CallInst>(&I);
  // Function* f = ci->getCalledFunction();
  // outs() << "Type: " << address->getType()->getTypeID() << "\n";
  
  Value* args[] = { mutex };


  // outs() << "pthread_mutex_unlock: lOGGING\n";
  Builder.CreateCall(llvm::cast<Function>(onMutexUnlock.getCallee()), args);
}



bool
Tolerator::runOnModule(Module& m) {
  // get types from context
  auto& context = m.getContext();

  voidTy = Type::getVoidTy(context);
  int1Ty = Type::getInt1Ty(context); 
  int32Ty = Type::getInt32Ty(context); // todo: 64?
  int64Ty = Type::getInt64Ty(context);

  int8PtrTy = Type::getInt8PtrTy(context);
  int32PtrTy = Type::getInt32PtrTy(context);


  // insert external functions
  initializeTracker = m.getOrInsertFunction("ToLeRaToR_initializeTracker", voidTy);

  registerMalloc = m.getOrInsertFunction("ToLeRaToR_registerMalloc", voidTy, int8PtrTy, int64Ty);
  registerAlloca = m.getOrInsertFunction("ToLeRaToR_registerAlloca", voidTy, int8PtrTy, int64Ty);
  unregisterAlloca = m.getOrInsertFunction("ToLeRaToR_unregisterAlloca", voidTy, int8PtrTy);

  // todo: rename these
  isValidLoadWithExit = m.getOrInsertFunction("ToLeRaToR_isValidLoadWithExit", voidTy, int8PtrTy);    // todo: size arg
  isValidStoreWithExit = m.getOrInsertFunction("ToLeRaToR_isValidStoreWithExit", voidTy, int8PtrTy);  // todo: size arg
  isValidFreeWithExit = m.getOrInsertFunction("ToLeRaToR_isValidFreeWithExit", voidTy, int8PtrTy);

  // todo: pass argument of current thread?
  onPthreadCreate = m.getOrInsertFunction("ToLeRaToR_onPthreadCreate", voidTy);
  onPthreadJoin = m.getOrInsertFunction("ToLeRaToR_onPthreadJoin", voidTy);
  onMutexLock = m.getOrInsertFunction("ToLeRaToR_onMutexLock", voidTy, int8PtrTy);
  onMutexUnlock = m.getOrInsertFunction("ToLeRaToR_onMutexUnlock", voidTy, int8PtrTy);

  registerIfNewThread = m.getOrInsertFunction("ToLeRaToR_registerIfNewThread", voidTy);
  

  // // todo: replace with better impl.
  //   std::vector<Instruction*> startOfFunctionInstructions;
  //   for (auto& F : m) {

  //     // printf("here\n");
  //     // BasicBlock& EntryBlock = (&F)->getEntryBlock();
  //     // // auto& EntryBlock = (&F)->get();
  //     // Instruction* FirstInstruction = &(*EntryBlock.getFirstInsertionPt());
  //     // startOfFunctionInstructions.push_back(FirstInstruction);

  //     // IRBuilder<> Builder(I);
  //     // Builder.CreateCall(llvm::cast<Function>(registerIfNewThread.getCallee()));

  //     // printf("here\n");

  //   // FunctionCallee helloFunc = F.getParent()->getOrInsertFunction("ToLeRaToR_helloworld", voidTy);
  //     // for (auto& B : F) {
  //     //   for (auto& I : B) {
  //     //     if (auto* callInst = dyn_cast<CallInst>(&I)) {
  //     //       outs() << "function name: " << callInst->getCalledFunction()->getName() << "\n";
  //     //       // outs() << "function: " << I.getFunction()->getName() << "\n";
  //     //     }
  //     //   }
  //     // }


  //     // if (F.isDeclaration()) {
  //     //   continue;
  //     // }

  //     // BasicBlock* EntryBB = &F.front();
  //     // IRBuilder<> Builder(EntryBB);
  //     // Builder.CreateCall(llvm::cast<Function>(registerIfNewThread.getCallee()));
  //   }

  // for (auto* I : startOfFunctionInstructions) {
  //   printf("here\n");
  //   IRBuilder<> Builder(I);
  //   Builder.CreateCall(llvm::cast<Function>(registerIfNewThread.getCallee()));
  // }


  // visit instructions and build work lists of instructions that need instrumenting
  InstrumentationVisitor visitor;
  visitor.visit(&m);

  // instrument instructions
  for (auto* I : visitor.mallocInstructions) {
    instrumentMallocInstruction(I);
  }
  for (auto* I : visitor.freeInstructions) {
    instrumentFreeInstruction(I);
  }
  for (auto* I : visitor.allocaInstructions) {
    instrumentAllocaInstruction(I);
  }
  for (auto* I : visitor.loadInstructions) {
    instrumentLoadInstruction(I);
  }
  for (auto* I : visitor.storeInstructions) {
    instrumentStoreInstruction(I);
  }
  for (auto* I : visitor.pthreadCreateInstructions) {
    instrumentPthreadCreateInstruction(I);
  }
  for (auto* I : visitor.pthreadJoinInstructions) {
    instrumentPthreadJoinInstruction(I);
  }
  for (auto* I : visitor.pthreadMutexLockInstructions) {
    instrumentPthreadMutexLockInstruction(I);
  }
  for (auto* I : visitor.pthreadMutexUnlockInstructions) {
    instrumentPthreadMutexUnlockInstruction(I);
  }



  // outs() << "hello hello\n";

  //   for (auto& F : m) {
  //   // FunctionCallee helloFunc = F.getParent()->getOrInsertFunction("ToLeRaToR_helloworld", voidTy);
  //     for (auto& B : F) {
  //       for (auto& I : B) {
  //         if (auto* callInst = dyn_cast<CallInst>(&I)) {
  //           outs() << "function name: " << callInst->getCalledFunction()->getName() << "\n";
  //           // outs() << "function: " << I.getFunction()->getName() << "\n";
  //         }
  //       }
  //     }
  //   }

  // std::vector<Instruction*> startOfFunctionInstructions;
  // for (auto& F : m) {
  //   Instruction& firstInst = *(F.begin()->begin());
  // }



  // get globals
  for (auto Global = (&m)->global_begin(); Global != (&m)->global_end(); ++Global) {
    GlobalVariable& G = *Global;
    DataLayout* DL = new DataLayout(&m);

    Instruction& firstInst = *(m.begin()->begin()->begin());
    IRBuilder<> Builder(&firstInst);
    
    int allocationSize; // todo: type?  i64?
    if (ArrayType* array = dyn_cast<ArrayType>(G.getValueType())) { 
      int arraySize = array->getNumElements();
      TypeSize typeSize = DL->getTypeAllocSize(array->getArrayElementType());
      allocationSize = arraySize * typeSize;
    } else {
      allocationSize = DL->getTypeAllocSize(G.getValueType());
    }

    Value* address = cast<Value>(&G);
    
    Value* size = Builder.getInt64(allocationSize);
    Value* args[] = { address, size };

    Builder.CreateCall(llvm::cast<Function>(registerAlloca.getCallee()), args);
  }

  // get global mutexes
  // for (auto global = (&m)->global_begin(); global != (&m)->global_end(); ++global) {
  //   // if (global->getType()->isStructTy() && global->getType()->getStructName() == "struct.__pthread_mutex_s") {
  //   //   outs() << "Found mutex: \n";
  //   //   outs() << "--" << global->getName() << "\n";
  //   // }

  //   if (global->getType()->isPointerTy()) {
  //     // outs() << "Pointer type\n";
  //     // outs() << "Name: " << global->getName() << "\n";
  //     if (global->getName() == "mutex") {
  //       outs() << "Type: " << global->getType() << "\n";
  //     }
  //     Type* elementType = global->getType()->getStructElementType(0);

  //     // if (global->getType()->getElementType()->isStructTy()) {
  //     //   outs() << "Struct type\n";
        
  //     //   if (global->getType()->getElementType()->getStructName() == "union.pthread_mutex_t") {
  //     //     outs() << "mutex detected: " << global->getName() << "\n";
  //     //   }
  //     // }

  //     // outs() << "Struct type: " << global->getType()->getStructName() << "\n";
  //   }

  //   // if (global->getType()->isAggregateType()) {
  //   //   outs() << "\n\nStruct type: \n\n";
  //   // }
  //   // outs() << "Type: " << global->getType()->isPointerTy() << "\n";
  //   // if (global->getType()->isPointerTy()) {
  //   //   outs() << "Pointer Type\n";
  //   // }
  // }

  // todo: refactor/remove
  appendToGlobalCtors(m, llvm::cast<Function>(initializeTracker.getCallee()), 0);


  // auto helloworld = m.getOrInsertFunction("ToLeRaToR_helloworld", voidTy);
  // appendToGlobalCtors(m, llvm::cast<Function>(helloworld.getCallee()), 0);


  // auto goodbyeworld = m.getOrInsertFunction("ToLeRaToR_goodbyeworld", voidTy);
  // appendToGlobalDtors(m, llvm::cast<Function>(goodbyeworld.getCallee()), 0);

  return true;
}