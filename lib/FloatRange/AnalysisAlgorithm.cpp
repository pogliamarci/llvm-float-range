#include "AnalysisAlgorithm.h"

#include "Range.h"
#include "OptionalValue.h"

#include "llvm/Analysis/ScalarEvolutionExpressions.h"

#include <set>
#include <queue>
#include <map>

using namespace llvm;
using namespace cto;

template <typename T>
void AnalysisAlgorithm<T>::Worklist::enqueue(llvm::Instruction *val) {
  // Do not add elements to the Worklist multiple times
  if (contained.find(val) != contained.end()) {
    return;
  }
  contained.insert(val);
  impl.push(val);
}

template <typename T>
llvm::Instruction *AnalysisAlgorithm<T>::Worklist::dequeue() {
  assert(!isEmpty() && "dequeue called on an empty worklist");
  Instruction *val = impl.front();
  contained.erase(val);
  impl.pop();
  return val;
}

template <typename T>
T AnalysisAlgorithm<T>::visit(llvm::Instruction *inst) {
  if (isa<llvm::BinaryOperator>(inst)) {
    BinaryOperator *bop = dyn_cast < BinaryOperator > (inst);
    BinaryOperator::BinaryOps opcode = bop->getOpcode();
    switch (opcode) {
    case BinaryOperator::FAdd:
      return visitFAdd(*bop);
      break;
    case BinaryOperator::FSub:
      return visitFSub(*bop);
      break;
    case BinaryOperator::FMul:
      return visitFMul(*bop);
      break;
    case BinaryOperator::FDiv:
      return visitFDiv(*bop);
      break;
    default:
      break;
    }
  } else if (isa < PHINode > (inst)) {
    PHINode *phi = dyn_cast < PHINode > (inst);
    return visitPhi(*phi);
  }
  report_fatal_error("Attempting to visit an unsupported instruction");
  return getUnboundedResult();
}

template <typename T>
void AnalysisAlgorithm<T>::analyze(const inst_iterator &begin,
		const inst_iterator &end) {
  for (inst_iterator it = begin; it != end; ++it) {
    wl.enqueue(&(*it));
  }

  while (!wl.isEmpty()) {
    Instruction *cur = wl.dequeue();

    // Ensure that the instruction is supported
    if (!isSupported(cur)) {
      continue;
    }

    // If we are in a loop, ensure we process each instruction at most the
    // tripcount, if it is statically known, or the maximum number of iterations
    // Note that this might produce incorrect results for complex structures
    // even though the tripcount is known!
    Loop *loop = loopInfo.getLoopFor(cur->getParent());
    if (loop != NULL) {
      const SCEVConstant *CC = dyn_cast<SCEVConstant>(scev.getMaxBackedgeTakenCount(loop));
      if (CC != NULL) {
        unsigned tripCount = CC->getValue()->getValue().getZExtValue();
        if (counter[cur] >= tripCount) {
          continue;
        }
      } else {
        // Loops with unknown tripcount are not supported.
    	// Default to unbounded value.
        // Don't check if the insertion succeeded, we are guaranteed
    	// that this is the first time we've seen this Value*.
        resultSet.insert(std::make_pair<Value *, T>(cur, getUnboundedResult()));
        continue;
      }
    }

#ifdef TRACE_VISITING
    errs() << "Visiting ";
    cur->print(errs());
    errs() << "\n";
#endif

    T result = visit(cur);

    for (Value::use_iterator u = cur->use_begin(), e = cur->use_end();
         u != e; ++u) {
      if (isa<Instruction>(*u)) {
        wl.enqueue(dyn_cast<Instruction>(*u));
      } else {
        report_fatal_error("Found uses that are not instructions. This is unsupported.");
      }
    }

    std::pair<typename result_set_t::iterator, bool> res =
      resultSet.insert(std::make_pair<Value *, T>(cur, result));
    if (!res.second) {
      res.first->second = result;
    }
    counter[cur]++;
  }
}

// Force explicit instantiation of template class
template class AnalysisAlgorithm<OptionalValue<double> >;
template class AnalysisAlgorithm<Range >;
