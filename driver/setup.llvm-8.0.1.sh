cmake -G "Unix Makefiles" \
-DLLVM_TARGETS_TO_BUILD="X86" \
-DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD="RISCV" \
-DLLVM_ENABLE_PROJECTS="clang;libunwind" \
-DCMAKE_BUILD_TYPE=Debug \
-DCMAKE_INSTALL_PREFIX=~/Documents/llvm-8.0.1/install-debug \
-DLLVM_BINUTILS_INCDIR=~/Documents/binutils/include \
-DBUILD_SHARED_LIBS=ON \
../llvm \

