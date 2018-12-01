#include "Utils.h"

#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "LLVM_TDG_UTILS"

std::unordered_map<const llvm::Function *, std::string>
    Utils::MemorizedDemangledFunctionNames;

const std::string &Utils::getDemangledFunctionName(const llvm::Function *Func) {
  auto MemorizedIter = Utils::MemorizedDemangledFunctionNames.find(Func);
  if (MemorizedIter == Utils::MemorizedDemangledFunctionNames.end()) {

    size_t BufferSize = 1024;
    char *Buffer = static_cast<char *>(std::malloc(BufferSize));
    std::string MangledName = Func->getName();
    int DemangleStatus;
    size_t DemangledNameSize = BufferSize;
    auto DemangledName = llvm::itaniumDemangle(
        MangledName.c_str(), Buffer, &DemangledNameSize, &DemangleStatus);

    if (DemangledName == nullptr) {
      DEBUG(llvm::errs() << "Failed demangling name " << MangledName
                         << " due to ");
      switch (DemangleStatus) {
      case -4: {
        DEBUG(llvm::errs() << "unknown error.\n");
        break;
      }
      case -3: {
        DEBUG(llvm::errs() << "invalid args.\n");
        break;
      }
      case -2: {
        DEBUG(llvm::errs() << "invalid mangled name.\n");
        break;
      }
      case -1: {
        DEBUG(llvm::errs() << "memory alloc failure.\n");
        break;
      }
      default: { llvm_unreachable("Illegal demangle status."); }
      }
      // When failed, we return the original name.
      MemorizedIter =
          Utils::MemorizedDemangledFunctionNames.emplace(Func, MangledName)
              .first;

    } else {
      // Succeeded.
      std::string DemangledNameStr(DemangledName);
      auto Pos = DemangledNameStr.find('(');
      if (Pos != std::string::npos) {
        DemangledNameStr = DemangledNameStr.substr(0, Pos);
      }
      MemorizedIter =
          Utils::MemorizedDemangledFunctionNames.emplace(Func, DemangledNameStr)
              .first;
    }

    if (DemangledName != Buffer || DemangledNameSize >= BufferSize) {
      // Realloc happened.
      BufferSize = DemangledNameSize;
      Buffer = DemangledName;
    }

    // Deallocate the buffer.
    std::free(Buffer);
  }

  return MemorizedIter->second;
}

#undef DEBUG_TYPE