# Install LLVM-TDG

## Without stdlib

### LLVM

Checkout LLVM 6.0 and build it. Build type must be "Debug", otherwise we can not use `-debug-only` flag.

```bash
cd LLVM_BUILD_ROOT
cmake -G "Unix Makefiles" \
-DCMAKE_BUILD_TYPE=Debug \
-DCMAKE_INSTALL_PREFIX=LLVM_INSTALL_ROOT \
-DLLVM_BINUTILS_INCDIR=BINUTILS_INCLUDE \
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
--with-zlib=no
make -j9
make install
```

### LibUnwind

The tracer runtime library relies on libunwind to recognize the stack frames when some non call/return control flows happen. Mainly due to exception thrown/caught. Make sure your compiler can find it.

### GLLVM

We need to use [gllvm](https://github.com/SRI-CSL/gllvm) to extract bitcode for the SPEC2017 benchmark. When using it, make sure that you set `LLVM_COMPILER_PATH` to the correct place.