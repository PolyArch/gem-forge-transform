
cd ~/Documents
mkdir -p riscv
cd riscv
rm -r install
# git clone --recursive https://github.com/riscv/riscv-gnu-toolchain
cd riscv-gnu-toolchain
./configure --prefix=${HOME}/Documents/riscv/install --enable-multilib
make clean
make -j9 
