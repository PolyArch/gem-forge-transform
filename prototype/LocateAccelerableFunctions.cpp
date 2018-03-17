#include "LocateAccelerableFunctions.h"

char LocateAccelerableFunctions::ID = 0;
static llvm::RegisterPass<LocateAccelerableFunctions> L(
    "locate-acclerable-functions", "Locate accelerable functions", false, true);