#ifndef CTO_PRECISION_ANALYSIS_H_
#define CTO_PRECISION_ANALYSIS_H_

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"

#include "OptionalValue.h"

#include "FloatRangeAnalysis.h"

#define WORD_LENGTH 64

namespace cto {

struct PrecisionAnalysis : public llvm::FunctionPass {
  static char ID;

  PrecisionAnalysis() : llvm::FunctionPass(ID) {

  }

  virtual bool runOnFunction(llvm::Function &F);

  virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const {
    AU.addRequired<FloatRangeAnalysis>();
    AU.addRequired<llvm::LoopInfo>();
    AU.addRequired<llvm::ScalarEvolution>();
    AU.setPreservesAll();
  }

  OptionalValue<double> getMaximumError(const llvm::Function &F) const;

  OptionalValue<uint64_t> getEquivalentBitwidth(const llvm::Function &F) const;

  uint64_t getInternalDBW(const llvm::Function &F) const;

  void printAll(const llvm::Function &F) const;

private:
  std::map<const llvm::Value *, OptionalValue<double> > errorMap;
  std::map<const llvm::Function *, OptionalValue<double> > maxErrors;

};

}

#endif
