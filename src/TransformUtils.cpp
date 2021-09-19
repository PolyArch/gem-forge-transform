#include "TransformUtils.h"

llvm::Constant *TransformUtils::insertStringLiteral(llvm::Module *Module,
                                                    const std::string &Str) {
  // Create the constant array.
  auto Array =
      llvm::ConstantDataArray::getString(Module->getContext(), Str, true);
  // Get the array type.
  auto ElementTy = llvm::IntegerType::get(Module->getContext(), 8);
  auto ArrayTy = llvm::ArrayType::get(ElementTy, Str.size() + 1);
  // Register a global variable.
  auto GlobalVariableStr = new llvm::GlobalVariable(
      *(Module), ArrayTy, true, llvm::GlobalValue::PrivateLinkage, Array,
      ".str");
  llvm::MaybeAlign AlignBytes(1);
  GlobalVariableStr->setAlignment(AlignBytes);
  /**
   * Get the address of the first element in the string.
   * Notice that the global variable %.str is also a pointer, that's why we
   * need two indexes. See great tutorial:
   * https://
   * llvm.org/docs/GetElementPtr.html#what-is-the-first-index-of-the-gep-instruction
   */
  auto ConstantZero = llvm::ConstantInt::get(
      llvm::IntegerType::getInt32Ty(Module->getContext()), 0, false);
  std::vector<llvm::Constant *> Indexes;
  Indexes.push_back(ConstantZero);
  Indexes.push_back(ConstantZero);
  return llvm::ConstantExpr::getGetElementPtr(ArrayTy, GlobalVariableStr,
                                              Indexes);
}