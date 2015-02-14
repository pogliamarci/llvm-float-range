#define DEBUG_TYPE "precision-analysis"

#include "PrecisionAnalysis.h"

#include "AnalysisAlgorithm.h"

#include "llvm/Support/InstIterator.h"
#include "llvm/InstVisitor.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/Debug.h"

#include <cmath>
#include <iostream>
#include <algorithm>
#include <queue>
#include <map>
#include <utility>

using namespace cto;
using namespace llvm;

char PrecisionAnalysis::ID = 0;
static RegisterPass<PrecisionAnalysis> X("precision-analysis", "Precision analysis", false, false);

namespace {

class PrecisionAnalysisAlgorithm : public AnalysisAlgorithm<OptionalValue<double> > {
public:

  PrecisionAnalysisAlgorithm(
    LoopInfo &loopInfo,
    ScalarEvolution &scev,
    FloatRangeAnalysis &floatRange,
    uint64_t decimalBitWidth) :
    AnalysisAlgorithm<OptionalValue<double> >(loopInfo, scev),
    FRA(floatRange),
    decimalBitWidth(decimalBitWidth),
    maxError(0.0) {
  }

  OptionalValue<double> getUnboundedResult() {
    return OptionalValue<double>::invalid();
  }

  OptionalValue<double> getMaxError() {
    return maxError;
  }

  virtual ~PrecisionAnalysisAlgorithm() {

  }

private:
  FloatRangeAnalysis &FRA;
  uint64_t decimalBitWidth;
  OptionalValue<double> maxError;

  OptionalValue<double> update(OptionalValue<double> val) {
    maxError = max(val, maxError);
    return val;
  }

  /* return the maximum absolute value in range of val*/
  OptionalValue<double> getRangeMax(Value *val) {
    Range r = FRA.getRange(val);
    if (r.isValid())
      return OptionalValue<double>(fmax(fabs(r.getMin()), fabs(r.getMax())));
    return OptionalValue<double>::invalid();
  }

  OptionalValue<double> getQuantError() {
    // Note: static_cast<int> is necessary because decimalBitWidth is an unsigned!
    return pow(2, (-1) * static_cast<int>(decimalBitWidth));
  }

  OptionalValue<double> getError(Value *val) {
    result_set_t::iterator found = resultSet.find(val);
    if (found != resultSet.end()) {
      return found->second;
    }
    if (isa<Constant>(val)) {
      if (ConstantFP *CFP = dyn_cast<ConstantFP>(val)) {
        // For constants, simulate conversion to fixed point
        // and use the difference as a precision loss
        const APFloat &val = CFP->getValueAPF();
        double dval = val.convertToDouble();
        uint64_t fixpoint = static_cast<uint64_t>(
                              dval * static_cast<double>(1 << decimalBitWidth));
        double converted = fixpoint / static_cast<double>(1 << decimalBitWidth);
        return dval - converted;
      }
      llvm_unreachable("Analysing floating point precision of a non-floating-point constant!");
      return -1;
    } else {
      // For variables, just assume a quantisation error for the current bit width
      return getQuantError();
    }
  }

  OptionalValue<double> visitFAdd(BinaryOperator &B) {
    Value *op1 = B.getOperand(0);
    Value *op2 = B.getOperand(1);
    OptionalValue<double> e1 = getError(op1);
    OptionalValue<double> e2 = getError(op2);
    return update(e1 + e2);
  }

  OptionalValue<double> visitFSub(BinaryOperator &B) {
    return visitFAdd(B);
  }

  OptionalValue<double> visitFMul(BinaryOperator &B) {
    Value *op1 = B.getOperand(0);
    Value *op2 = B.getOperand(1);
    OptionalValue<double> e1 = getError(op1);
    OptionalValue<double> e2 = getError(op2);
    return update(getRangeMax(op1) * e2 + getRangeMax(op2)
                  * e1 + e1 * e2 + getQuantError());
  }

  OptionalValue<double> visitFDiv(BinaryOperator &B) {
    Value *op1 = B.getOperand(0);
    Value *op2 = B.getOperand(1);
    OptionalValue<double> e1 = getError(op1);
    OptionalValue<double> e2 = getError(op2);
    return update(getRangeMax(op1) / pow(getRangeMax(op2), 2) * e2
                  + 1.0 / getRangeMax(op2) * e1 + getQuantError());
  }

  OptionalValue<double> visitPhi(PHINode &P) {
    OptionalValue<double> max = 0.0;
    for (unsigned int i = 0; i < P.getNumOperands(); ++i) {
      OptionalValue<double> e = getError(P.getOperand(i));
      if (e.isValid() && max.isValid() && e.get() > max.get())
        max = e;
    }
    return max;
  }
};
}

bool PrecisionAnalysis::runOnFunction(Function &F) {
  FloatRangeAnalysis &FRA = getAnalysis<FloatRangeAnalysis>();
  LoopInfo &LI = getAnalysis<LoopInfo>();
  ScalarEvolution &scev = getAnalysis<ScalarEvolution>();

  uint64_t decimalBitWidth = getInternalDBW(F);

  PrecisionAnalysisAlgorithm algorithm(LI, scev, FRA, decimalBitWidth);

  algorithm.analyze(inst_begin(F), inst_end(F));

  maxErrors.insert(std::make_pair<Function *, OptionalValue<double> >(
                     &F,
                     algorithm.getMaxError()));

  PrecisionAnalysisAlgorithm::result_set_t res = algorithm.getResult();
  for (PrecisionAnalysisAlgorithm::result_set_t::iterator el = res.begin(),
       end = res.end(); el != end; ++el) {
    errorMap.insert(*el);
  }

  DEBUG(printAll(F));

  return false; /* analysis pass */
}

OptionalValue<double> PrecisionAnalysis::getMaximumError(const Function &F) const {
  std::map<const Function *, OptionalValue<double> >::const_iterator found = maxErrors.find(&F);
  if (found != maxErrors.end()) {
    return found->second;
  }
  llvm_unreachable("Error mapping for the function should always exist!");
  return -1;
}

OptionalValue<uint64_t> PrecisionAnalysis::getEquivalentBitwidth(const Function &F) const {
  OptionalValue<double> err = getMaximumError(F);
  if (err.isValid() && err.get() > 0.0) {
    double bw = fmax(0.0, ceil(log2(1 / err.get())));
    return OptionalValue<uint64_t>(static_cast<uint64_t>(bw));
  }
  return OptionalValue<uint64_t>::invalid();
}

uint64_t PrecisionAnalysis::getInternalDBW(const Function &F) const {
  FloatRangeAnalysis &FRA = getAnalysis<FloatRangeAnalysis>();

  OptionalValue<uint64_t> integerBitWidth = FRA.getMinimumIntegerBitWidth(F);
  uint64_t decimalBitWidth = integerBitWidth.isValid() ?
                             (WORD_LENGTH - integerBitWidth.get()) / 2 : 0;

  DEBUG(errs() << "Integer BW: " << integerBitWidth
        << " ; Decimal BW : " << decimalBitWidth << "\n");

  return decimalBitWidth;
}

void PrecisionAnalysis::printAll(const Function &F) const {
  errs() << "Printing precision for the function " << F.getName() << "\n\n";
  for (const_inst_iterator BI = inst_begin(F), BE = inst_end(F); BI != BE; ++BI) {
    const Value *I = &(*BI);
    std::map<const Value *, OptionalValue<double> >::const_iterator res = errorMap.find(I);
    if (res != errorMap.end()) {
      errs() << res->second;
    }
    I->print(errs());
    errs() << '\n';
  }

  errs() << "Maximum error: " << getMaximumError(F)
         << " (equivalent decimal bitwidth: " <<  getEquivalentBitwidth(F) << ")\n";
  errs() << "\n-- \n";
}

