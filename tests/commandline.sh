#!/bin/bash

#
# Simple invocation of llvm-float-range, for the purpose of
# executing tests.
# Please set the LLVM_BUILD environment variable to the directory where you built LLVM
#

if [ -z "$1" ]; then
    echo "Usage: ${0} <file.c>"; exit 1;
fi

PRECISION=8
if [ -n "$2" ]; then
    PRECISION=${2}
fi

if [ -z "$LLVM_BUILD" ]; then
    echo "Please set the LLVM_BUILD environment variable to your llvm build directory"; exit  1;
fi

OUTDIR="$LLVM_BUILD/Release+Asserts"
if [ -d "$LLVM_BUILD/Debug+Asserts" ]; then
    OUTDIR="$LLVM_BUILD/Debug+Asserts"
fi

FLOATRANGEDIR="$LLVM_BUILD/projects/float-range/Release+Asserts"
if [ -d "$LLVM_BUILD/projects/float-range/Debug+Asserts" ]; then
    FLOATRANGEDIR="$LLVM_BUILD/projects/float-range/Debug+Asserts"
fi

"$OUTDIR/bin/clang" -emit-llvm "${1}" -c -o "${1}.bc"

"$OUTDIR/bin/opt" -load "$FLOATRANGEDIR/lib/LLVMFloatRange.so" -stats -mem2reg -lcssa -float-range-analysis -precision-analysis -float2fix -dce -time-passes -debug  -precision-bitwidth "$PRECISION" -S < ${1}.bc > ${1}.ll 2> ${1}.log

rm "${1}.bc"

"$OUTDIR/bin/clang" "${1}" -o "${1}.out.orig"

echo "Original program:"
"./${1}.out.orig"

echo "Transformed program:"
"$OUTDIR/bin/lli" "${1}.ll"

rm "${1}.out.orig"
