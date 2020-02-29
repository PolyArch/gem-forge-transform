#ifndef GEM_FORGE_TRANSFORM_UTILS_H
#define GEM_FORGE_TRANSFORM_UTILS_H

/**
 * Some utils for transform a llvm IR.
 */

#include "Utils.h"

#include "llvm/Analysis/TargetTransformInfo.h"

class TransformUtils {
public:
  /**
   * Dead code elimination.
   */
  static bool eliminateDeadCode(llvm::Function &F,
                                llvm::TargetLibraryInfo *TLI);
  static bool simplifyCFG(llvm::Function &F, llvm::TargetLibraryInfo *TLI,
                          llvm::TargetTransformInfo &TTI);

  static llvm::Constant *insertStringLiteral(llvm::Module *Module,
                                             const std::string &Str);
};

#endif
