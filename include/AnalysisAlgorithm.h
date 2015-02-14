#ifndef CTO_ANALYSIS_ALGORITHM_H_
#define CTO_ANALYSIS_ALGORITHM_H_

#include "llvm/IR/Instruction.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Support/InstIterator.h"

#include <map>
#include <queue>

namespace cto {

template<typename T>
class AnalysisAlgorithm {
public:
  typedef std::map<llvm::Value *, T> result_set_t;

protected:
  result_set_t resultSet;

private:
  struct Worklist {
  private:
    std::set<llvm::Instruction *> contained;
    std::queue<llvm::Instruction *> impl;
  public:
    void enqueue(llvm::Instruction *val);
    llvm::Instruction *dequeue();
    inline bool isEmpty() const {
      return impl.empty();
    }
  };

  llvm::LoopInfo &loopInfo;
  llvm::ScalarEvolution &scev;
  Worklist wl;
  std::map<llvm::Value *, unsigned> counter;

  virtual T visitFAdd(llvm::BinaryOperator &B) = 0;
  virtual T visitFSub(llvm::BinaryOperator &B) = 0;
  virtual T visitFMul(llvm::BinaryOperator &B) = 0;
  virtual T visitFDiv(llvm::BinaryOperator &B) = 0;
  virtual T visitPhi(llvm::PHINode &B) = 0;
  virtual T getUnboundedResult() = 0;

  bool isBinaryOperatorSupported(const llvm::BinaryOperator &bop) const {
    switch (bop.getOpcode()) {
    case llvm::BinaryOperator::FAdd:
    case llvm::BinaryOperator::FSub:
    case llvm::BinaryOperator::FMul:
    case llvm::BinaryOperator::FDiv:
      return true;
      break;
    default:
      return false;
    }
  }

public:
  AnalysisAlgorithm(llvm::LoopInfo &loopInfo, llvm::ScalarEvolution &scev) :
    loopInfo(loopInfo), scev(scev) {
  }

  bool isSupported(llvm::Instruction *inst) {
    if (llvm::isa<llvm::PHINode>(inst)) {
      return inst->getOperand(0)->getType()->isFloatingPointTy();
    } else if (llvm::isa<llvm::BinaryOperator>(inst)) {
      return isBinaryOperatorSupported(*llvm::dyn_cast<llvm::BinaryOperator>(inst));
    }
    return false;
  }

  T visit(llvm::Instruction *inst);

  void analyze(const llvm::inst_iterator &begin, const llvm::inst_iterator &end);

  inline result_set_t getResult() {
    return resultSet;
  }

  virtual ~AnalysisAlgorithm() {
  }
};

}

#endif
