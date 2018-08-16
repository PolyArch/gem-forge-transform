# LLVM-TDG

## Complete Tool Chain

### ELLCC

It is important that we can trace into the standard library to get a more complete LLVM datagraph, therefore we have to compile the standard library into LLVM IR and insert trace instructions into it. However, the GNU C library and C++ library can only be compiled with GCC as they use many GNU extensions.

Therefore, we use [ellcc](http://ellcc.org/) as a complete toolchain for our research. After download the binary, we let it fetch all the sources it requires.

```bash
wget http://ellcc.org/releases/latest/ellcc-x86_64-linux-2017-08-23.bz2
tar xfp ellcc-x86_64-linux-2017-08-23.bz2
cd ellcc
# Install the binary into ./ellcc/bin
./ellcc install
# This will actuall build everything from scratch and gives us opt, llvm-link, etc.
./ellcc build
```

Before we proceed, we set some environment variables to be used in the following commands, and also add `ellcc` to our `PATH`. You can also add this to your `.bashrc`.

```bash
export ELLCC_PATH=/path/to/ellcc
export PATH=$ELLCC_PATH/bin:$PATH
```

### LLVM

Now have a complete toolchain which is compatible with LLVM. However, the default `ellcc build` command will produce a statically linked `opt`, which can not load shared library for our passes. Therefore we have to build a dynamically linked `opt`, with assertion enabled so that we can print debug information.

```bash
cd $ELLCC_PATH/build/build-ecc/llvm/x86_64-linux
# Remove the current build.
make clean

# Configure llvm to build shared library.
CC="ecc -target x86_64-linux" \
CXX="ecc++ -target x86_64-linux" \
cmake -G "Unix Makefiles" \
-DCMAKE_INSTALL_PREFIX=$ELLCC_PATH/build/local \
-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=$ELLCC_PATH/bin \
-DLLVM_LIBXML2_ENABLED=OFF \
-DLLVM_ENABLE_LIBXML2=OFF \
-DLLVM_ENABLE_ASSERTIONS=ON \
-DCMAKE_BUILD_TYPE=Debug \
$ELLCC_PATH/build/src/llvm \
-DCMAKE_C_FLAGS=' -DELLCC_ARG0=\"x86_64-linux\"' \
-DCMAKE_CXX_FLAGS=' -DELLCC_ARG0=\"x86_64-linux\"'

# Build opt.
make opt -j9
# Fix the interpreter.
$ELLCC_PATH/bin/patchelf --set-interpreter $ELLCC_PATH/libecc/lib/x86_64-linux/libc.so bin/opt
# Copy to binary folder.
cp bin/opt ../../../../bin
```

### GLLVM

We need to use [gllvm](https://github.com/SRI-CSL/gllvm) to compile the standard library into LLVM IR. It is straight forward to set it up. Notice that when we using it, we are going to set the `LLVM_CC_NAME=ecc`, `LLVM_CXX_NAME=ecc++`, `LLVM_AR_NAME=ecc-ar`, `LLVM_LINK_NAME=llvm-link`.

### Compile MUSL to LLVM IR

```bash
# Set up the environment for gllvm.
export LLVM_COMPILER_PATH=$ELLCC_PATH/bin
export LLVM_CC_NAME=ecc
export LLVM_CXX_NAME=ecc++
export LLVM_AR_NAME=ecc-ar
export LLVM_LINK_NAME=llvm-link
mkdir ELLCC_PATH/build/build-gllvm-musl/
cd ELLCC_PATH/build/build-gllvm-musl
# Configure it.
CC="gclang -target x86_64-linux" \
CFLAGS= \
CROSS_COMPILE=$ELLCC_PATH/bin/ecc- \
LDFLAGS=-L$ELLCC_PATH/libecc/lib/x86_64-linux \
LIBCC=-lcompiler-rt \
$ELLCC_PATH/build/src/musl/configure \
--target=x86_64-linux \
--enable-warnings \
--enable-debug \
--enable-depends \
--prefix=$ELLCC_PATH/build/local/musl/x86_64-linux \
--syslibdir=$ELLCC_PATH/build/local/musl/x86_64-linux/lib
# Make it.
make -j9
# Extract the llvm bytecode. Will produce libc.a.bc
cd lib
get-bc -b libc.a
```

If you see a warning as:

```bash
WARNING:Did not recognize the compiler flag: -target
WARNING:Did not recognize the compiler flag: x86_64-linux
```

You can just safely ignore it. This is due to the parser of gllvm failed to match these flags to a patten. It will still pass those flags to the compiler.

### Compile LibC++ to LLVM IR

### Compile Protobuf 3.5.1 with LibC++

Since we are using a completely different toolchain, we also need to compile protobuf with that toolchain. Notice that we have to build protobuf without RTTI information to link with LLVM.

```bash
# Configure it.
CC=ecc \
CXX=ecc++ \
CXX_FOR_BUILD=ecc++ \
CPPFLAGS=-DGOOGLE_PROTOBUF_NO_RTTI \
CXXFLAGS=-fPIC \
./configure \
--prefix=/home/zhengrong/Documents/protobuf-ellcc \
--enable-shared=no \
--with-zlib=no
# Make it.
make -j9
make install
```

### Set Up LLVM-TDG

To set up LLVM-TDG.

```bash
mkdir build
cd build
cmake ..
make
```

Check it with MachSuite.

```bash
cd ../benchmark
python MachSuite.py MachSuite
```

### LLVM Trace CPU

