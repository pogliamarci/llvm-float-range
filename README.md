CTO -- Code Transformation and Optimization Project (A.Y. 2013/2014)
====================================================================

This directory contains the project developed for the CTO course at Politecnico
di Milano, A.Y. 2013/2014.

Aim of the project is to develop a set of LLVM passes that, starting from a
specification of the ranges of the local - floating point - variables, are able
to convert the code to fixed point with a predictable loss of precision.

Quick Start
-----------

The project is a set of analysis and transformation passes for the LLVM
compiler.
To create IR with the metadata used within the project, it is necessary to
compile a modified version of `clang` as per the following instructions.

The project was developed and tested with LLVM 3.4 under Linux (in particular,
it is known to work with CentOS\RHEL 6 using clang 3.4 compiled from sources
using the stock gcc 4.4.6 from the repositories).

In the following, we assume that
* `LLVM_SRC` is the directory where the llvm source code is located
* `LLVM_BUILD` is the llvm build directory (separated from the source tree)

Download the LLVM 3.4 and clang sources:

    $ wget http://llvm.org/releases/3.4/llvm-3.4.src.tar.gz
    $ tar xvf llvm-3.4.src.tar.gz
    $ mv llvm-3.4 $LLVM_SRC
    $ rm llvm-3.4.src.tar.gz
    $ cd $LLVM_SRC
    $ wget http://llvm.org/releases/3.4/clang-3.4.src.tar.gz
    $ tar xvf clang-3.4.src.tar.gz
    $ mv clang-3.4 tools/clang
    $ rm clang-3.4.src.tar.gz

Clone this repository in the projects/ directory:

    $ git clone https://github.com/pogliamarci/llvm-float-range $LLVM_SRC/projects/float-range

Apply the provided patch (adding support for the float_range attribute):

    $ cd $LLVM_SRC
    $ patch -p1 < projects/float-range/llvm-float-range.patch
    $ cd tools/clang
    $ patch -p1 < $LLVM_SRC/projects/float-range/clang-float-range.patch

Configure LLVM, clang and the float-range project in `LLVM_BUILD`:

    $ mkdir -p $LLVM_BUILD
    $ cd $LLVM_BUILD
    $ $LLVM_SRC/configure
    $ mkdir -p $LLVM_BUILD/projects/float-range
    $ cd $LLVM_BUILD/projects/float-range
    $ $LLVM_SRC/projects/float-range/configure
    
Finally, configure and compile it all:

    $cd $LLVM_BUILD
    $ make

When running `configure`, you can as usual specify an installation prefix using
`--prefix`, and install the binaries running `make install`. If you have a
multi-core machine, you can speed up compilation passing the usual `-j $N`
parameter to `make`. Beware that the LLVM and clang compilation and linking
process require quite an high amount of memory.

The LLVM build system should automatically compile llvm and clang in release
mode (`$LLVM_BUILD/Release+Asserts/{bin,lib}`), and the float-range project in
debug mode (`$LLVM_BUILD/projects/float-range/Debug+Asserts/lib`). The build
moden is chosen based on the directory being a repository checkout or not
(i.e., checks for `.git` or `.svn` directories).

Structure
---------

Some notes about the structure and functionalities of the project are in the
`doc` folder.

