
cd ~/Documents
mkdir -p riscv
cd riscv
# git clone --recursive https://github.com/riscv/riscv-gnu-toolchain
cd riscv-gnu-toolchain
./configure --prefix=${HOME}/Documents/riscv/install-linux-gnu --with-arch=rv32gc --with-abi=ilp32d
make clean
make -j9 