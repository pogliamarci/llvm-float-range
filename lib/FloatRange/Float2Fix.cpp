#define DEBUG_TYPE "float2fix"

#include "PrecisionAnalysis.h"

#include "llvm/Pass.h"
#include "llvm/ADT/APSInt.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/Dominators.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"
#include "llvm/InstVisitor.h"
#include "llvm/Support/CommandLine.h"

#include <vector>
#include <map>

using namespace cto;
using namespace std;
using namespace llvm;

STATISTIC(Float2FixConverted,
          "Number of instructions converted from float to fixed point");
STATISTIC(ValuesReconvertedToFloat,
          "Number of values converted back from fixed to float");

static cl::opt<unsigned> DecimalPrecision("precision-bitwidth", cl::init(16),
    cl::desc("float2fix: Precision requested for the decimal part in order"
             "to enable conversion to fixed point. This is expressed as"
             "the number of \"equivalent\" decimal bits."
             "For this optionto take effect, the PrecisionAnalyis pass"
             "should be executed before the float2fix-analysis pass."));
static cl::opt<unsigned> InternalBitWidth("internal-bitwidth", cl::init(200),
    cl::desc("float2fix: Specify the number of internal bitwidth."
             "In this case all the operations are converted to fixed point"
             "provided they do not overflow (for what concerns the integer part."
             "This option is provided just as a debug convenience."));

namespace {

typedef std::map<Value *, Value *> inst_cache_t;

static inline bool rangeOk(Range range, uint64_t integerBW) {
  uint64_t limit = 1 << (integerBW - 1);
  return range.isValid() &&
         (-1 * range.getMin()) < limit && range.getMax() < limit;
}

struct Float2Fix : public FunctionPass {
  static char ID;

  Float2Fix() : FunctionPass(ID),
    DecimalBitWidth(0),
    Precision(OptionalValue<uint64_t>::invalid()),
    FRA(NULL),
    PRA(NULL) {}

  virtual bool runOnFunction(Function &F);

  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<PrecisionAnalysis>();
    AU.addRequired<FloatRangeAnalysis>();
    AU.addRequired<DominatorTree>();
    AU.setPreservesAll();
  }
private:
  uint64_t DecimalBitWidth;
  OptionalValue<uint64_t> Precision;
  FloatRangeAnalysis *FRA;
  PrecisionAnalysis *PRA;

  void convertParametersToFloat(
    Instruction *inst,
    inst_cache_t &converted_values,
    inst_cache_t &converted_back);

  void printConverted(const Function &F) const;

  bool okToConvert(const Function &F, const Instruction *inst, bool usePrecisionAnalysis) const {
    if (usePrecisionAnalysis) {
      if (!Precision.isValid() || Precision.get() < DecimalPrecision.getValue())
        return false;

      if ((isa<FCmpInst>(inst) && FRA->getRange(inst->getOperand(0)).isValid()
           && FRA->getRange(inst->getOperand(1)).isValid()))
        return true;

      return FRA->getRange(inst) != Range::Top;
    } else {
      uint64_t integerBW = WORD_LENGTH - 2 * DecimalBitWidth;

      if (isa<FCmpInst>(inst) || !rangeOk(FRA->getRange(inst), integerBW))
        return false;

      for (Instruction::const_op_iterator ops = inst->op_begin(),
           opend = inst->op_end(); ops != opend; ++ops) {
        if (!rangeOk(FRA->getRange(ops->get()), integerBW))
          return false;
      }
      return true;
    }
  }
};

struct ConverterVisitor : public InstVisitor<ConverterVisitor> {

  ConverterVisitor(uint64_t decimalBitWidth) :
    decimalBitWidth(decimalBitWidth) {}

  void visitFAdd(BinaryOperator &B) {
    vector<Value *> FixedPointOperands = convertOperands(B);
    BinaryOperator *converted = BinaryOperator::CreateAdd(
                                  FixedPointOperands.at(0),
                                  FixedPointOperands.at(1),
                                  "fixadd");
    converted->insertAfter(&B);
    convertedValues[&B] = converted;
  }

  void visitFSub(BinaryOperator &B) {
    vector<Value *> FixedPointOperands = convertOperands(B);
    BinaryOperator *converted;
    converted = BinaryOperator::CreateSub(
                  FixedPointOperands.at(0),
                  FixedPointOperands.at(1),
                  "fixsub");
    converted->insertAfter(&B);
    convertedValues[&B] = converted;
  }

  void visitFMul(BinaryOperator &B) {
    vector<Value *> FixedPointOperands = convertOperands(B);
    ConstantInt *ShiftAmount(ConstantInt::get(
                               IntegerType::get(B.getContext(), WORD_LENGTH),
                               decimalBitWidth,
                               true));
    BinaryOperator *mul = BinaryOperator::CreateMul(
                            FixedPointOperands.at(0),
                            FixedPointOperands.at(1),
                            "fixmul");
    BinaryOperator *converted = BinaryOperator::CreateAShr(
                                  mul,
                                  ShiftAmount,
                                  "fixashr");

    mul->insertAfter(&B);
    converted->insertAfter(mul);
    convertedValues[&B] = converted;
  }

  void visitFDiv(BinaryOperator &B) {
    vector<Value *> FixedPointOperands = convertOperands(B);
    ConstantInt *ShiftAmount(ConstantInt::get(
                               IntegerType::get(B.getContext(), WORD_LENGTH),
                               decimalBitWidth,
                               true));
    BinaryOperator *sft = BinaryOperator::CreateShl(
                            FixedPointOperands.at(0),
                            ShiftAmount,
                            "sft");
    BinaryOperator *converted = BinaryOperator::CreateSDiv(
                                  sft,
                                  FixedPointOperands.at(1),
                                  "fixdiv");
    sft->insertAfter(&B);
    converted->insertAfter(sft);
    convertedValues[&B] = converted;
  }

  void visitPHI(PHINode &P) {
    vector<Value *> values = convertOperands(P);
    PHINode *converted = PHINode::Create(
                           values.at(0)->getType(),
                           values.size(),
                           "fixphi");
    for (unsigned i = 0; i < P.getNumIncomingValues(); ++i) {
      converted->addIncoming(values.at(i), P.getIncomingBlock(i));
    }
    converted->insertAfter(&P);
    convertedValues[&P] = converted;
  }

  void visitFCmp(FCmpInst &B) {
    vector<Value *> FixedPointOperands = convertOperands(B);
    CmpInst::Predicate pred = B.getPredicate();
    if (CmpInst::isFPPredicate(pred)) {
      pred = convertPredicate(pred);
    }
    CmpInst *converted = CmpInst::Create(CmpInst::ICmp,
                                         pred,
                                         FixedPointOperands.at(0),
                                         FixedPointOperands.at(1),
                                         "conv-icmp");
    converted->insertAfter(&B);
    convertedValues[&B] = converted;
  }

  void visitInstruction(Instruction &I) {
    report_fatal_error("Attempting to convert an unsupported operation!");
  }

  inst_cache_t getConvertedValues() {
    return convertedValues;
  }

private:
  uint64_t decimalBitWidth;
  inst_cache_t convertedValues;

  CmpInst::Predicate convertPredicate(CmpInst::Predicate pred) {
    if (CmpInst::isIntPredicate(pred)) {
      return pred;
    }
    switch (pred) {
    case CmpInst::FCMP_OEQ:
    case CmpInst::FCMP_UEQ:
      return CmpInst::ICMP_EQ;
      break;
    case CmpInst::FCMP_OGT:
    case CmpInst::FCMP_UGT:
      return CmpInst::ICMP_SGT;
      break;
    case CmpInst::FCMP_OGE:
    case CmpInst::FCMP_UGE:
      return CmpInst::ICMP_SGE;
      break;
    case CmpInst::FCMP_OLT:
    case CmpInst::FCMP_ULT:
      return CmpInst::ICMP_SLT;
      break;
    case CmpInst::FCMP_OLE:
    case CmpInst::FCMP_ULE:
      return CmpInst::ICMP_SLE;
      break;
    case CmpInst::FCMP_ONE:
    case CmpInst::FCMP_UNE:
      return CmpInst::ICMP_NE;
      break;
    case CmpInst::FIRST_FCMP_PREDICATE:
      return CmpInst::FIRST_ICMP_PREDICATE;
      break;
    case CmpInst::LAST_FCMP_PREDICATE:
      return CmpInst::LAST_ICMP_PREDICATE;
      break;
    case CmpInst::BAD_FCMP_PREDICATE:
      return CmpInst::BAD_ICMP_PREDICATE;
      break;
    default:
      report_fatal_error("Attempting to convert unsupported predicate");
      return CmpInst::BAD_ICMP_PREDICATE;
    }
  }

  Value *floatToFixed(Value *operand) {
    if (isa<llvm::Constant>(operand)) {
      return floatToFixedConstant(dyn_cast<llvm::Constant>(operand));
    }

    assert(operand->getType()->isFloatTy() || operand->getType()->isDoubleTy());

    ConstantFP *factor_cfp = ConstantFP::get(
                               operand->getContext(),
                               APFloat(static_cast<double>((1 << decimalBitWidth))));
    Instruction *double_mul = BinaryOperator::CreateFMul(
                                operand,
                                factor_cfp,
                                "conv-fmul");
    Instruction *cast = new FPToSIInst(
      double_mul,
      IntegerType::get(operand->getContext(), 64), "conv-cast");

    if (Instruction *insertionPoint = dyn_cast<Instruction>(operand)) {
      double_mul->insertAfter(insertionPoint);
      cast->insertAfter(double_mul);
    } else if (Argument *arg = dyn_cast<Argument>(operand)) {
      arg->getParent()->front().getInstList().push_front(cast);
      arg->getParent()->front().getInstList().push_front(double_mul);
    } else {
      operand->print(errs());
      errs() << " is neither an Instruction nor an Argument!\n";
      abort();
    }
    return cast;
  }

  std::vector<Value *> convertOperands(Instruction &I) {
    std::vector<Value *> FixedPointOperands;
    for (User::op_iterator ops = I.op_begin(), opend = I.op_end();
         ops != opend; ++ops) {
      inst_cache_t::iterator cached = convertedValues.find(ops->get());
      if (cached == convertedValues.end()) { // cache miss
        Value *converted = floatToFixed(ops->get());
        convertedValues[ops->get()] = converted;
        FixedPointOperands.push_back(converted);
      } else {                               // cache hit
        FixedPointOperands.push_back(cached->second);
      }
    }
    return FixedPointOperands;
  }

  Value *floatToFixedConstant(Constant *operand) {
    ConstantFP *operand_fp = dyn_cast<ConstantFP>(operand);
    assert(operand_fp != NULL);
    const APFloat &value = operand_fp->getValueAPF();
    double d = value.convertToDouble();
    return ConstantInt::get(
             IntegerType::get(operand->getContext(), 64),
             d * (1 << decimalBitWidth), true);
  }

};

}

static Value *fixedToFloatConstant(Constant *operand, uint64_t DecimalBitWidth) {
  ConstantInt *operand_fixpoint = dyn_cast<ConstantInt>(operand);
  assert(operand_fixpoint != NULL);
  int64_t value = operand_fixpoint->getSExtValue();
  double d = (double) value / (1 << DecimalBitWidth);
  return ConstantFP::get(Type::getDoubleTy(operand->getContext()), d);
}

static Value *fixedToFloat(Value *operand, uint64_t DecimalBitWidth) {
  if (isa<llvm::Constant>(operand)) { //XXX is this useful?
    return fixedToFloatConstant(
             dyn_cast<llvm::Constant>(operand),
             DecimalBitWidth);
  }
  assert(operand->getType()->isIntegerTy());
  Instruction *cast = new SIToFPInst(
    operand,
    Type::getDoubleTy(operand->getContext()), "convfp-cast");
  ConstantFP *factor_cfp = ConstantFP::get(
                             operand->getContext(),
                             APFloat(static_cast<double>((1 << DecimalBitWidth))));
  Instruction *div = BinaryOperator::CreateFDiv(
                       cast,
                       factor_cfp,
                       "convfp-fdiv");

  if (Instruction *i = dyn_cast<Instruction>(operand)) {
    div->insertAfter(i);
    cast->insertAfter(i);
  } else {
    // by construction, a value originally float but converted to fixed
    // can only be an Instruction, and not an Argument.
    operand->print(errs());
    errs() << " is not an Instruction!\n";
    abort();
  }
  return div;
}

char Float2Fix::ID = 0;
static RegisterPass<Float2Fix> X(
  "float2fix",
  "Float to fixed point conversion",
  false,
  false);

bool Float2Fix::runOnFunction(Function &F) {

  bool IRChanged = false;

  PRA = &getAnalysis<PrecisionAnalysis>();
  FRA = &getAnalysis<FloatRangeAnalysis>();
  Precision = PRA->getEquivalentBitwidth(F);

  bool usePrecisionAnalysis = true;
  DecimalBitWidth = PRA->getInternalDBW(F);
  if (InternalBitWidth.getValue() <=  WORD_LENGTH) {
    DecimalBitWidth = InternalBitWidth.getValue();
    usePrecisionAnalysis = false;
  }

  DEBUG(printConverted(F));

  //1. Generate FixedPoint versions of the values that according to the
  //   range analysis can be converted with a negligible loss of precision
  ConverterVisitor visitor(DecimalBitWidth);
  for (inst_iterator Itr = inst_begin(F), IEnd = inst_end(F);
       Itr != IEnd; ++Itr) {
    Instruction *Inst = &(*Itr);
    if (okToConvert(F, Inst, usePrecisionAnalysis)) {
      // Convert the instruction inst and its parameters to fixed point.
      // converted_values: cache of previously converted parameters:
      //                   if a parameter was previously converted, we use that version
      // Note: The converted_values cache works only if the definition of the previously
      //       converted value is a predecessor of the current instruction. This is the
      //       case when this function is called while visiting the function in a topological
      //       order thanks to the LLVM IR being in SSA form.
      visitor.visit(*Inst);
      IRChanged = true;
      ++Float2FixConverted;
    }
  }
  inst_cache_t ConvertedValues = visitor.getConvertedValues();

  //2. Now scan the code again for the instruction that have not been
  //   converted, and generate a floating point version of the parameters
  //   that are now converted to fixed point.
  inst_cache_t ConvertedBackToFloatValues;
  for (inst_iterator Itr = inst_begin(F), IEnd = inst_end(F);
       Itr != IEnd; ++Itr) {
    Instruction *Inst = &(*Itr);
    if (!okToConvert(F, Inst, usePrecisionAnalysis)) {
      // ensure it has not been converted
      if (ConvertedValues.find(Inst) == ConvertedValues.end())
        convertParametersToFloat(Inst, ConvertedValues, ConvertedBackToFloatValues);
    }
  }

  return IRChanged;
}

// Convert the parameters of inst, previously converted to fixed point, to float again
void Float2Fix::convertParametersToFloat(Instruction *inst,
    inst_cache_t &converted_values,
    inst_cache_t &converted_back) {

  DominatorTree &DOM = getAnalysis<DominatorTree>();

  for (unsigned i = 0; i < inst->getNumOperands(); ++i) {
    Value *operand = inst->getOperand(i);
    // convert only instructions: parameters and globals are born floating point
    if (isa<Instruction>(operand) && converted_values.count(operand) > 0) {
      inst_cache_t::iterator val = converted_back.find(operand);
      if (val != converted_back.end()) {
        inst->setOperand(i, val->second);
      } else {
        Value *fixedPoint = converted_values[operand];
        // Compare instructions have a boolean result, no need to convert them back again!
        if (isa<CmpInst>(fixedPoint)) {
          inst->setOperand(i, fixedPoint);
        } else {
          ++ValuesReconvertedToFloat;
          // Sanity check, although the condition should be always true
          // because the fixed point conversion generates an use right
          // after the floating point definition.
          if (DOM.dominates(dyn_cast<Instruction>(fixedPoint), inst)) {
            Value *converted;
            converted = fixedToFloat(fixedPoint, DecimalBitWidth);
            converted_back[operand] = converted;
            inst->setOperand(i, converted);
          } else {
            errs() << "Parameter not converted due to domination issues."
                   "Keeping the original floating point version. \n";
          }
        }
      }
    }
  }
}

void Float2Fix::printConverted(const Function &F) const {
  errs() << "Printing float2fix results for function " << F.getName() << "\n\n";
  for (const_inst_iterator BI = inst_begin(F), BE = inst_end(F); BI != BE; ++BI) {
    const Instruction *I = &(*BI);
    if (okToConvert(F, I, (InternalBitWidth.getValue() <=  WORD_LENGTH))) {
      errs() << "[CONVERT] ";
    } else {
      errs() << "[  KEEP ] ";
    }
    I->print(errs());
    errs() << '\n';
  }
  errs() << "\n-- \n";
}
