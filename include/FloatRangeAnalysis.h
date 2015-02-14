#ifndef CTO_FLOAT_RANGE_ANALYSIS_H_
#define CTO_FLOAT_RANGE_ANALYSIS_H_

#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Constants.h"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/Dominators.h"

#include "Range.h"
#include "OptionalValue.h"

#include <map>

namespace cto {

typedef std::map<const llvm::Value *, cto::Range> range_store_t;

struct FloatRangeAnalysis : public llvm::FunctionPass {
  static char ID;

  FloatRangeAnalysis() : llvm::FunctionPass(ID) {
  }
  virtual bool runOnFunction(llvm::Function &F);

  virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const {
    AU.addRequired<llvm::ScalarEvolution>();
    AU.addRequired<llvm::LoopInfo>();
    AU.addRequired<llvm::DominatorTree>();
    AU.setPreservesAll();
  }
  void printAll(const llvm::Function &F) const;

  cto::Range getRange(const llvm::Value *val) {
    if (const llvm::ConstantFP *CFP = llvm::dyn_cast<const llvm::ConstantFP>(val)) {
      return Range(CFP->getValueAPF().convertToDouble());
    } else {
      std::map<const llvm::Value *, Range>::iterator found = Store.find(val);
      if (found != Store.end()) {
        return found->second;
      }
    }
    return Range::Top;
  }

  OptionalValue<uint64_t> getMinimumIntegerBitWidth(const llvm::Function &F) const;

private:
  range_store_t Store;
  std::map<const llvm::Function *, OptionalValue<uint64_t> > minimumBits;
  bool propagate(llvm::Instruction *II);
  OptionalValue<uint64_t> computeMinimumBits(const llvm::Function &F);
  OptionalValue<uint64_t> computeBitsForValue(const llvm::Value &I);
};

}

#endif
