# Install LLVM-TDG

## Without stdlib

### LLVM 8.0.1

This repo should also work with LLVM 8.0.1. Checkout the repo.

```bash
git clone git@github.com:llvm/llvm-project.git llvm-8.0.1
cd llvm-8.0.1
git checkout llvmorg-8.0.1
```

In order to use the golden linker plugin we have download the include file.

```bash
git clone --depth 1 git://sourceware.org/git/binutils-gdb.git binutils
```

Build it. Build type must be "Debug", otherwise we can not use `-debug-only` flag. Notice that we also build shared library to avoid redefinition. We also specify the include directory of binutils to build the golden linker plugin.

When using LLVM 8.0.1, there are some issues when using `InductionDescriptor::isInductionPHI()` and `RecurrenceDescriptor::isReductionPHI()`. The problem is that these two functions assume that there will be a loop preheader if the value has AddRecSCEV. However, this is not the case in LLVM 8.0.1. So far I have to change it to use `getLoopPredecessor()` insteand of `getLoopPreheader()`, which is more relaxed than the later.

Similarily you want to build both Debug and Release configuration. You can use the provided `driver/setup.llvm-8.0.1.sh`.

### Protobuf 3.5.1

Checkout protobuf 3.5.1 and build it. Notice that LLVM is built without RTTI, so we have specify the same flag for protobuf so that we can link them together. Also, we have to make it position independent to be statically linked into a shared library.

```bash
cd PROTOBUF_SRC_ROOT
CPPFLAGS=-DGOOGLE_PROTOBUF_NO_RTTI \
CXXFLAGS=-fPIC \
./configure \
--enable-shared=no \
--with-zlib=yes
make -j9
make install
```

### GemForgeCPU

Get the gem5 and build it. Some required packages:

```bash
sudo apt install python-dev m4
```

### Python Environment

We suggest using Anaconda to manager the environment. The driver is written in Python 2.7 and requires numpy and sklearn to perform simpoint. So after installing anaconda:

```bash
conda activate ENVIRONMENT
conda install numpy
conda install scikit-learn
conda install enum34
```

Remember to setup the python version of protobuf. If you are using Anaconda to manager the environment, type this:

```bash
cd PROTOBUF_SRC_ROOT/python
python setup.py install --prefix ANACONDA/envs/YOUR_ENV
```

### Environment Variable

This project relies on some environment variables to run. You can add this to the activation script of your anaconda environment.

```bash
export LLVM_DEBUG_INSTALL_PATH=/where/debug/llvm/installed
export LLVM_RELEASE_INSTALL_PATH=/where/release/llvm/installed
export LLVM_SRC_LIB_PATH=/where/llvm/source/tree/lib/is
export LIBUNWIND_INC_PATH=/where/libunwind/include/dir/is
export LIBUNWIND_LIB_PATH=/where/is/libunwind.a
export LLVM_TDG_PATH=/where/this/project/is
export LLVM_TDG_RESULT_PATH=/where/the/result/is
```

### Example

We provide a script as an example of end-to-end workflow. Suppose you have [CortexSuite](http://cseweb.ucsd.edu/groups/bsg/) installed. With these additional environment variable.

```bash
export LLVM_TRACE_CPU_PATH=/where/llvm/trace/cpu/simulator/is
export SDVBS_SUITE_PATH=/where/SD-VBS/suite/is
```
