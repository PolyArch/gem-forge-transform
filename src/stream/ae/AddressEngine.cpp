
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"

#include "ExecutionEngine/Interpreter/Interpreter.h"

#include <string>
#include <vector>

namespace {
llvm::cl::opt<std::string> BitCodeFile(llvm::cl::desc("Input bitcode file."),
                                       llvm::cl::Positional);
}

std::unique_ptr<llvm::Module> readBitCodeFile(llvm::LLVMContext &Context,
                                              const std::string &BitCodeFile) {
  llvm::SMDiagnostic Err;
  return llvm::parseIRFile(BitCodeFile, Err, Context);
}

void *interpret(llvm::Interpreter &Interpreter, const std::string &FuncName) {

  auto FuncPtr = Interpreter.FindFunctionNamed(FuncName);
  assert(FuncPtr != nullptr && "Failed to find the callee.");

  llvm::GenericValue Arr;
  llvm::GenericValue Idx;
  Arr.PointerVal = reinterpret_cast<llvm::PointerTy>(0x128);
  Idx.IntVal = llvm::APInt(32, 1, true);

  std::vector<llvm::GenericValue> Args{Arr, Idx};
  auto Result = Interpreter.runFunction(FuncPtr, Args);

  return Result.PointerVal;
}

int main(int argc, char *argv[]) {

  llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
  llvm::PrettyStackTraceProgram X(argc, argv);

  llvm::cl::ParseCommandLineOptions(argc, argv,
                                    "Start stream address engine.\n");

  llvm::LLVMContext Context;
  auto Module = readBitCodeFile(Context, BitCodeFile.getValue());
  assert(Module != nullptr && "Failed to parse llvm bitcode file.\n");

  llvm::Interpreter Interpreter(std::move(Module));

  llvm::errs() << interpret(Interpreter, "addr") << '\n';

  return 0;
}