#include "FunctionalStream.h"
#include "FunctionalStreamEngine.h"

#define DEBUG_TYPE "FunctionalStream"
FunctionalStream::FunctionalStream(Stream *_S, FunctionalStreamEngine *_SE)
    : S(_S), SE(_SE), Pattern(S->getPatternPath()) {
  DEBUG(llvm::errs() << "Initialized FunctionalStream of " << S->formatName()
                     << '\n');

  /**
   * Find the base functional streams.
   */
  for (const auto &BS : this->S->getChosenBaseStreams()) {
    auto FuncBS = this->SE->getNullableFunctionalStream(BS);
    assert(FuncBS != nullptr && "Failed to find the base stream.");
    this->BaseStreams.emplace(FuncBS->S->getInst(), FuncBS);
    FuncBS->DependentStreams.insert(this);
  }
}

void FunctionalStream::configure(DataGraph *DG) {
  this->Pattern.configure();
  if (!this->BaseStreams.empty()) {
    // We should compute the value from the base stream.
    assert(this->S->Type == Stream::TypeT::MEM &&
           "Only mem stream can have base streams.");
    this->CurrentValue = this->computeAddress(DG);
  } else {
    // This is a direct stream.
    this->CurrentValue = this->Pattern.getNextValue().second;
  }
  DEBUG(llvm::errs() << "Configure FunctionalStream of " << S->formatName()
                     << " with value " << this->CurrentValue << '\n');
}

void FunctionalStream::step(DataGraph *DG) {

  static int StackDepth = 0;
  StackDepth++;

  if (!this->BaseStreams.empty()) {
    // We should compute the value from the base stream.
    assert(this->S->Type == Stream::TypeT::MEM &&
           "Only mem stream can have base streams.");
    this->CurrentValue = this->computeAddress(DG);
  } else {
    // This is a direct stream.
    this->CurrentValue = this->Pattern.getNextValue().second;
  }
  DEBUG(llvm::errs() << "Step FunctionalStream of " << this->S->formatName()
                     << " with value " << this->CurrentValue << '\n');
  // Also step all the dependent streams.
  if (StackDepth == 100) {
    llvm::errs() << "Step stack depth reaches 100, there should be a circle in "
                    "the stream dependence graph.";
    assert(false && "Circle detected in the stream dependence graph as the "
                    "step stack depth is too high.");
  }

  for (auto &DependentStream : this->DependentStreams) {
    DEBUG(llvm::errs() << "Trigger step of dependent FunctionalStream of "
                       << DependentStream->S->formatName() << '\n');
    DependentStream->step(DG);
  }

  StackDepth--;
}

uint64_t FunctionalStream::computeAddress(DataGraph *DG) const {

  auto MS = static_cast<MemStream *>(this->S);
  const auto FuncName = MS->getAddressFunctionName();
  const auto &AddrDG = MS->getAddressDataGraph();

  auto &Interpreter = this->SE->getInterpreter();

  auto Func = Interpreter->FindFunctionNamed(FuncName);
  assert(Func != nullptr &&
         "Failed to find the address computing function in the module.");

  std::vector<llvm::GenericValue> Args;
  for (const auto &Input : AddrDG.getInputs()) {
    Args.emplace_back();

    auto BaseStreamsIter = this->BaseStreams.find(Input);
    if (BaseStreamsIter != this->BaseStreams.end()) {
      // This comes from the base stream.
      this->setGenericValueFromUInt64(Input->getType(), Args.back(),
                                      BaseStreamsIter->second->getValue());
    } else {
      // This is an input value.
      const DynamicValue *DynamicVal =
          DG->DynamicFrameStack.front().getValueNullable(Input);
      assert(DynamicVal != nullptr &&
             "Failed to look up the dynamic value of the input.");
      this->setGenericValueFromDynamicValue(Input->getType(), Args.back(),
                                            *DynamicVal);
    }
  }

  auto Result = Interpreter->runFunction(Func, Args);
  return this->extractAddressFromGenericValue(AddrDG.getAddrValue()->getType(),
                                              Result);
}

void FunctionalStream::setGenericValueFromDynamicValue(
    llvm::Type *Type, llvm::GenericValue &GenericVal,
    const DynamicValue &DynamicVal) const {
  switch (Type->getTypeID()) {
  case llvm::Type::PointerTyID: {
    GenericVal.PointerVal =
        reinterpret_cast<void *>(std::stoul(DynamicVal.Value, 0, 16));
    break;
  }
  case llvm::Type::IntegerTyID: {
    auto IntegerType = llvm::cast<llvm::IntegerType>(Type);
    unsigned BitWidth = IntegerType->getBitWidth();
    uint64_t Val = std::stoul(DynamicVal.Value);
    GenericVal.IntVal = llvm::APInt(BitWidth, Val);
    break;
  }
  default: {
    llvm_unreachable(
        "Unsupported llvm type to be translated into generic value.");
  }
  }
}

void FunctionalStream::setGenericValueFromUInt64(llvm::Type *Type,
                                                 llvm::GenericValue &GenericVal,
                                                 const uint64_t &Val) const {
  switch (Type->getTypeID()) {
  case llvm::Type::PointerTyID: {
    GenericVal.PointerVal = reinterpret_cast<void *>(Val);
    break;
  }
  case llvm::Type::IntegerTyID: {
    auto IntegerType = llvm::cast<llvm::IntegerType>(Type);
    unsigned BitWidth = IntegerType->getBitWidth();
    GenericVal.IntVal = llvm::APInt(BitWidth, Val);
    break;
  }
  default: {
    llvm_unreachable(
        "Unsupported llvm type to be translated into generic value.");
  }
  }
}

uint64_t FunctionalStream::extractAddressFromGenericValue(
    llvm::Type *Type, llvm::GenericValue &GenericVal) const {
  // To be honest, the address should always have pointer type.
  assert(Type->isPointerTy() &&
         "The address result should always be of pointer type.");
  return reinterpret_cast<uint64_t>(GenericVal.PointerVal);
}

#undef DEBUG_TYPE