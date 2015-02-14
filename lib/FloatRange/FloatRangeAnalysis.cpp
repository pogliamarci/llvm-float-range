#define DEBUG_TYPE "float-range-analysis"

#include "FloatRangeAnalysis.h"

#include "OptionalValue.h"
#include "AnalysisAlgorithm.h"

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/InstVisitor.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"

#include <iostream>
#include <vector>
#include <map>
#include <deque>
#include <list>
#include <cmath>
#include <algorithm>
#include <utility>

using namespace cto;
using namespace llvm;

char FloatRangeAnalysis::ID = 0;
static RegisterPass<FloatRangeAnalysis> X("float-range-analysis",
    "Floating point range analysis", false, false);

namespace {

struct CtrlDep {
  FCmpInst *condition;
  BasicBlock *truePath;
  BasicBlock *falsePath;
};

struct FloatRangeAlgorithm : public AnalysisAlgorithm<Range> {

  FloatRangeAlgorithm(LoopInfo &loopInfo,
                      ScalarEvolution &scev,
                      DominatorTree &domTree,
                      std::map<Value *, std::vector<CtrlDep> > controlDeps,
                      result_set_t &initialRanges) :
    AnalysisAlgorithm(loopInfo, scev),
    LI(loopInfo),
    domTree(domTree),
    controlDependencies(controlDeps) {
    for (result_set_t::iterator it = initialRanges.begin(),
         end = initialRanges.end(); it != end; ++it) {
      resultSet.insert(*it);
    }
  }

  Range visitFAdd(BinaryOperator &B) {
    Range r1 = getOperandRange(B.getOperand(0), B);
    Range r2 = getOperandRange(B.getOperand(1), B);
    Range ret = r1 + r2;
#ifdef TRACE_FLOAT_RANGE_ANALYSIS
    errs() << r1 << " + " << r2 << " = " << ret << "\n";
#endif
    return ret;
  }

  Range visitFSub(BinaryOperator &B) {
    Range r1 = getOperandRange(B.getOperand(0), B);
    Range r2 = getOperandRange(B.getOperand(1), B);
    Range ret = r1 - r2;
#ifdef TRACE_FLOAT_RANGE_ANALYSIS
    errs() << r1 << " - " << r2 << " = " << ret << "\n";
#endif
    return ret;
  }

  Range visitFMul(BinaryOperator &B) {
    Range r1 = getOperandRange(B.getOperand(0), B);
    Range r2 = getOperandRange(B.getOperand(1), B);
    Range ret = r1 * r2;
#ifdef TRACE_FLOAT_RANGE_ANALYSIS
    errs() << r1 << " * " << r2 << " = " << ret << "\n";
#endif
    return ret;
  }

  Range visitFDiv(BinaryOperator &B) {
    Range r1 = getOperandRange(B.getOperand(0), B);
    Range r2 = getOperandRange(B.getOperand(1), B);
    Range ret = r1 / r2;
#ifdef TRACE_FLOAT_RANGE_ANALYSIS
    errs() << r1 << " / " << r2 << " = " << ret << "\n";
#endif
    return ret;
  }

  Range visitPhi(PHINode &PH) {
    if (LI.isLoopHeader(PH.getParent()) && !visited[&PH]) {
      // First time of the phi instruction in the loop header
      // => consider only first operand
      visited[&PH] = true;
      Range r = Range::Top;
      bool foundSomething = false;
      for (unsigned int i = 0; i < PH.getNumOperands(); ++i) {
        Value *operand = PH.getOperand(i);
        // Avoid considering backedges in the first iteration of the loop
        if (!isa<Instruction>(operand)
            || domTree.dominates(dyn_cast<Instruction>(operand), &PH)) {
          foundSomething = true;
          Range curRange = getOperandRange(operand, PH);
          if (r == Range::Top) {
            r = curRange;
          } else {
            r = r | curRange;
          }
        }
      }
      if (!foundSomething) {
        report_fatal_error("Phi instruction without an entry block");
      }
#ifdef TRACE_FLOAT_RANGE_ANALYSIS
      errs() << "first " << r << "\n";
#endif
      return r;
    } else {
      Range rx = getOperandRange(PH.getOperand(0), PH);
      for (unsigned int i = 1; i < PH.getNumOperands(); ++i)
        rx = rx | getOperandRange(PH.getOperand(i), PH);
#ifdef TRACE_FLOAT_RANGE_ANALYSIS
      errs() << "union: " << rx << " \n";
#endif
      return rx;
    }
  }

  Range getUnboundedResult() {
    return Range::Top;
  }

private:

  LoopInfo &LI;
  DominatorTree &domTree;
  std::map<Value *, std::vector<CtrlDep> > controlDependencies;
  std::map<PHINode *, bool> visited;

  Range constrainRange(Range &r, Value *operand, Value *condition, bool isTrue) {
    if (isa<FCmpInst>(condition)) {
      FCmpInst *cmpCondition = dyn_cast<FCmpInst>(condition);
      CmpInst::Predicate pred = isTrue ? cmpCondition->getPredicate()
                                : cmpCondition->getInversePredicate();
      Value *other = operand == cmpCondition->getOperand(0) ?
                     cmpCondition->getOperand(1) : cmpCondition->getOperand(0);
      Range otherRange = getOperandRange(other, *cmpCondition);

      if (otherRange == Range::Top) // Can't say anything
        return r;

      if (operand == cmpCondition->getOperand(1)) {
        pred = CmpInst::getInversePredicate(pred);
      }

      switch (pred) {
      case FCmpInst::FCMP_UGT:
      case FCmpInst::FCMP_OGT:
      case FCmpInst::FCMP_UGE:
      case FCmpInst::FCMP_OGE: {
        r = Range(::fmax(r.getMin(), otherRange.getMin()), r.getMax());
#ifdef TRACE_FLOAT_RANGE_ANALYSIS
        errs() << "Constraining: " << constrainedRange << '\n';
#endif
        break;
      }
      case FCmpInst::FCMP_ULT:
      case FCmpInst::FCMP_OLT:
      case FCmpInst::FCMP_ULE:
      case FCmpInst::FCMP_OLE: {
        r = Range(r.getMin(), ::fmin(r.getMax(), otherRange.getMax()));
#ifdef TRACE_FLOAT_RANGE_ANALYSIS
        errs() << "Constraining: " << constrainedRange << '\n';
#endif
        break;
      }
      // FP equalities and inequalities... not so meaningful
      case FCmpInst::FCMP_UEQ:
      case FCmpInst::FCMP_OEQ:
      case FCmpInst::FCMP_UNE:
      case FCmpInst::FCMP_ONE:
      default:
        break;
      }
    }
    return r;
  }

  Range getOperandRange(Value *val, Instruction &context) {
    if (isa<Constant>(val)) {
      if (ConstantFP *CFP = dyn_cast<ConstantFP>(val)) {
        const APFloat &val = CFP->getValueAPF();
        return Range(val.convertToDouble());
      }
    } else {
      result_set_t::iterator found = resultSet.find(val);
      if (found != resultSet.end()) {
        return refineWithControlDependencies(found->second, val, context);
      }
    }
    return Range::Top;
  }

  Range refineWithControlDependencies(Range r, Value *val, Instruction &context) {
    // See if there is a control dependency involving this value...
    std::map<Value *, std::vector<CtrlDep> >::iterator dependency =
      controlDependencies.find(val);
    if (dependency != controlDependencies.end()) {
      for (std::vector<CtrlDep>::iterator it = dependency->second.begin();
           it != dependency->second.end(); ++it) {
        // ... that is effective during the current instruction ...
        if (it->truePath
            && (it->truePath == context.getParent()
                || domTree.dominates(it->truePath, context.getParent()))) {
          r = constrainRange(r, val, it->condition, true);
        } else if (it->falsePath
                   && (it->falsePath == context.getParent()
                       || domTree.dominates(it->falsePath, context.getParent()))) {
          r = constrainRange(r, val, it->condition, false);
        }
      }
    }
    return r;
  }
};
}

OptionalValue<uint64_t> FloatRangeAnalysis::computeBitsForValue(const Value &V) {
  if (Store.find(&V) != Store.end() || isa<const llvm::ConstantFP>(V)) {
    Range r = getRange(&V);
    if (r.isBottom()) {
      return 0;  // this most likely is an always false condition...
    }
    if (r.isValid()) {
      // Note: we assume a two's complement fixed point representation as a target.
      // For the sake of simplicity, we also consider 64 bits as the maximum possible word length.
      // ceil() is because the approximation of the decimal part may add one unit to the integer part
      // The "+1" if get{Min,Max}() is positive is because two's complement representation
      // has the range [-2^N, +2^N-1].
      double rmin = ::ceil(r.getMin() < 0 ? ::abs(r.getMin()) : r.getMin() + 1);
      double rmax = ::ceil(r.getMax() < 0 ? ::abs(r.getMax()) : r.getMax() + 1);
      uint64_t bitwidth = static_cast<uint64_t>(::ceil(::log2(::fmax(rmin, rmax))) + 1);
      return OptionalValue<uint64_t>(bitwidth);
    } else {
      return OptionalValue<uint64_t>::invalid();
    }
  }
  // else, fake 0 minimum bits for instructions not supported
  // (function calls, integer, annotations, ...), so that max is not affected
  return OptionalValue<uint64_t>(0);
}

OptionalValue<uint64_t> FloatRangeAnalysis::computeMinimumBits(const Function &F) {
  OptionalValue<uint64_t> res(0);
  for (const_inst_iterator Itr = inst_begin(F), IEnd = inst_end(F);
       Itr != IEnd; ++Itr) {
    //XXX avoid visiting unsupported instructions
    //    (although this is needed now to consider operands to floating point
    //     comparisons, which will be converted)
    res = max(computeBitsForValue(*Itr), res);
    for (Instruction::const_op_iterator ops = Itr->op_begin(), opend =
           Itr->op_end(); ops != opend; ++ops) {
      res = max(computeBitsForValue(*(ops->get())), res);
    }
    if (!res.isValid())
      break;
  }
  return res;
}

bool FloatRangeAnalysis::runOnFunction(Function &F) {

  ScalarEvolution &SCEV = getAnalysis<ScalarEvolution>();
  LoopInfo &LI = getAnalysis<LoopInfo>();
  DominatorTree &DomTree = getAnalysis<DominatorTree>();

  // Initialization
  FloatRangeAlgorithm::result_set_t knownRanges;
  std::map<Value *, std::vector<CtrlDep> > controlDependencies;
  for (inst_iterator Itr = inst_begin(F), IEnd = inst_end(F); Itr != IEnd; ++Itr) {

    // Populate the data structure to refine the ranges according to branch conditions
    if (BranchInst *BI = dyn_cast<BranchInst>(&(*Itr))) {
      if (BI->isConditional()) {
        Value *Cond = BI->getCondition();
        if (isa<FCmpInst>(Cond)) {
          FCmpInst *Binop = dyn_cast<FCmpInst>(Cond);
          CtrlDep dep;
          dep.condition = Binop;
          dep.truePath = BI->getSuccessor(0);
          dep.falsePath = BI->getSuccessor(1);
          // If the true block can be executed without executing the false edge, we cannot say anything
          if (dep.truePath->getSinglePredecessor() == NULL) {
            dep.truePath = NULL;
          }
          // If the false block can be executed without executing the false edge, we cannot say anything
          if (dep.falsePath->getSinglePredecessor() == NULL) {
            dep.falsePath = NULL;
          }
          // Sanity check
          if (dep.truePath != dep.falsePath && (dep.truePath || dep.falsePath)) {
            // we add the constraint to all the operands predicated by this condition
            controlDependencies[Binop->getOperand(0)].push_back(dep);
            controlDependencies[Binop->getOperand(1)].push_back(dep);
          }
        }
      }
    }

    // Initialization -- retrieve initial range information from intrinsics
    // Parse llvm.float.range attribute
    if (Itr->getOpcode() != Instruction::Call)
      continue;
    Value *calledFunction = Itr->getOperand(Itr->getNumOperands() - 1);
    if (calledFunction->getName().str().find("llvm.float.range") != 0) {
      continue;
    }
    Value *annotatedValue = Itr->getOperand(0);
    ConstantInt *CIMin = dyn_cast<llvm::ConstantInt>(Itr->getOperand(1));
    ConstantInt *CIMax = dyn_cast<llvm::ConstantInt>(Itr->getOperand(2));
    assert(CIMin != NULL && CIMax != NULL && "Min and max are not ConstantInts!");
    Range r(CIMin->getSExtValue(), CIMax->getSExtValue());
    knownRanges[annotatedValue] = r;
  }

  FloatRangeAlgorithm algorithm(LI, SCEV, DomTree, controlDependencies, knownRanges);

  algorithm.analyze(inst_begin(F), inst_end(F));

  FloatRangeAlgorithm::result_set_t res = algorithm.getResult();
  for (FloatRangeAlgorithm::result_set_t::iterator it = res.begin(), end = res.end(); it != end; ++it) {
    Store.insert(std::make_pair<const Value *, Range>(it->first, it->second));
  }

  minimumBits.insert(std::make_pair<const Function *, OptionalValue<uint64_t> >(&F, computeMinimumBits(F)));

  DEBUG(printAll(F));

  return false; /* analysis pass, do not change anything */
}

OptionalValue<uint64_t> FloatRangeAnalysis::getMinimumIntegerBitWidth(const Function &F) const {
  std::map<const Function *, OptionalValue<uint64_t> >::const_iterator found = minimumBits.find(&F);
  if (found != minimumBits.end()) {
    return found->second;
  }
  return OptionalValue<uint64_t>::invalid();
}

void FloatRangeAnalysis::printAll(const Function &F) const {
  errs() << "Printing ranges for the function " << F.getName() << "\n\n";
  for (const_inst_iterator BI = inst_begin(F), BE = inst_end(F); BI != BE; ++BI) {
    const Instruction *I = &(*BI);
    std::map<const llvm::Value *, Range>::const_iterator res = Store.find(I);
    if (res != Store.end()) {
      errs() << res->second;
    }
    I->print(errs());
    errs() << '\n';
  }
  errs() << "Minimum integer bitwidth: " << getMinimumIntegerBitWidth(F) << '\n';
  errs() << "\n-- \n";
}
