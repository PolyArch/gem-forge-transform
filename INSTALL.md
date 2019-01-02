# Install LLVM-TDG

## Without stdlib

### LLVM

Checkout LLVM 6.0. The rest is optional for now.

```bash
mkdir llvm-6.0
cd llvm-6.0
svn co http://llvm.org/svn/llvm-project/llvm/tags/RELEASE_601/final/ llvm
cd llvm/tools
svn co http://llvm.org/svn/llvm-project/cfe/tags/RELEASE_601/final/ clang
cd ../runtimes
svn co http://llvm.org/svn/llvm-project/libunwind/tags/RELEASE_601/final/ libunwind
```

Build it. Build type must be "Debug", otherwise we can not use `-debug-only` flag. Notice that we also build shared library to avoid redefinition. We also specify the include directory of binutils to build the golden linker plugin.

```bash
cd LLVM_BUILD_DEBUG_ROOT
cmake -G "Unix Makefiles" \
-DCMAKE_BUILD_TYPE=Debug \
-DCMAKE_INSTALL_PREFIX=LLVM_INSTALL_DEBUG_ROOT \
-DLLVM_BINUTILS_INCDIR=BINUTILS_INCLUDE \
-DBUILD_SHARED_LIBS=ON \
LLVM_SRC_ROOT
make -j9
make install
```

Also build a release version.

```bash
cd LLVM_BUILD_DEBUG_ROOT
cmake -G "Unix Makefiles" \
-DCMAKE_BUILD_TYPE=Release \
-DCMAKE_INSTALL_PREFIX=LLVM_INSTALL_RELEASE_ROOT \
-DLLVM_BINUTILS_INCDIR=BINUTILS_INCLUDE \
-DBUILD_SHARED_LIBS=ON \
LLVM_SRC_ROOT
make -j9
make install
```

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

### GLLVM

We need to use [gllvm](https://github.com/SRI-CSL/gllvm) to extract bitcode for the SPEC2017 benchmark. When using it, make sure that you set `LLVM_COMPILER_PATH` to the correct place.

### Environment Variable

This project relies on some environment variables to run.

```bash
export LLVM_DEBUG_INSTALL_PATH=/where/debug/llvm/installed
export LLVM_RELEASE_INSTALL_PATH=/where/release/llvm/installed
export LLVM_SRC_LIB_PATH=/where/llvm/source/tree/lib/is
export LIBUNWIND_INC_PATH=/where/libunwind/include/dir/is
export LIBUNWIND_LIB_PATH=/where/is/libunwind.a
export LLVM_TDG_PATH=/where/this/project/is
```

### Example

We provide a script as an example of end-to-end workflow. Suppose you have [CortexSuite](http://cseweb.ucsd.edu/groups/bsg/) installed. With these additional environment variable.

```bash
export LLVM_TRACE_CPU_PATH=/where/llvm/trace/cpu/simulator/is
export SDVBS_SUITE_PATH=/where/SD-VBS/suite/is
```