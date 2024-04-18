
#pragma once


#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"


namespace tolerator {


enum class AnalysisType {
  LOGGING,      //   DEFAULT (??)  (Task 1) -> 0
  IGNORING,     //  -ignore   (Task 2)  -> 1
  DEFAULTING,   //  -defaults (Task 3)  -> 2
  BYPASSING     //  -returns  (Task 4)  -> 3
};

struct Tolerator : public llvm::ModulePass {
  static char ID;

  Tolerator(AnalysisType aType) : llvm::ModulePass(ID) 
  {
    ANALYSIS_TYPE = aType;

    printf("Analysis type: %d\n", (int)ANALYSIS_TYPE);
  }

  // should return true if module was modified, false otherwise
  bool runOnModule(llvm::Module& m) override;


  void instrumentLoadInstruction(llvm::Instruction* I);
  void instrumentStoreInstruction(llvm::Instruction* I);
  void instrumentFreeInstruction(llvm::Instruction* I);

  void instrumentPthreadCreateInstruction(llvm::Instruction* I);
  void instrumentPthreadJoinInstruction(llvm::Instruction* I);
  void instrumentPthreadMutexLockInstruction(llvm::Instruction* I);
  void instrumentPthreadMutexUnlockInstruction(llvm::Instruction* I);

// private:
  AnalysisType ANALYSIS_TYPE;
};


}  // namespace tolerator


