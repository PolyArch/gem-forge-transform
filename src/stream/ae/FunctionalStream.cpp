#include "FunctionalStream.h"
#include "FunctionalStreamEngine.h"

#include "llvm/Support/Format.h"

#define DEBUG_TYPE "FunctionalStream"
FunctionalStream::FunctionalStream(Stream *_S, FunctionalStreamEngine *_SE)
    : S(_S), SE(_SE), Pattern(S->getPatternPath()),
      HistorySerializer(_S->getHistoryPath()),
      HistoryTextFStream(_S->getHistoryTextPath()), CurrentIdx(InvalidIdx),
      IsAddressValid(false), IsValueValid(false) {
  DEBUG(llvm::errs() << "Initialized FunctionalStream of " << S->formatName()
                     << '\n');

  assert(this->HistoryTextFStream.is_open() &&
         "Failed to open the history text ofstream.");

  /**
   * Set the id field for the history protobuf message.
   */
  this->ProtobufHistoryEntry.set_id(this->S->getStreamId());

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
void FunctionalStream::DEBUG_DUMP(llvm::raw_ostream &OS) const {
  OS << this->S->formatName() << " idx "
     << llvm::format_hex(this->CurrentIdx, 18) << " addr "
     << llvm::format_hex(this->CurrentAddress, 18) << " value "
     << llvm::format_hex(this->CurrentValue, 18) << " valid "
     << this->IsValueValid;
}

void FunctionalStream::configure(DataGraph *DG) {
  this->Pattern.configure();
  this->CurrentIdx = InvalidIdx;
  this->CurrentIdx++;
  this->update(DG);
  DEBUG(llvm::errs() << "Configured ");
  DEBUG(this->DEBUG_DUMP(llvm::errs()));
  DEBUG(llvm::errs() << '\n');
}

void FunctionalStream::step(DataGraph *DG) {

  static int StackDepth = 0;
  StackDepth++;

  this->CurrentIdx++;
  this->update(DG);
  DEBUG(llvm::errs() << "Stpped ");
  DEBUG(this->DEBUG_DUMP(llvm::errs()));
  DEBUG(llvm::errs() << '\n');
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

void FunctionalStream::updateLoadedValue(DataGraph *DG,
                                         const DynamicValue &DynamicVal) {
  assert(this->S->Type == Stream::TypeT::MEM &&
         "Function updatedLoadedValue should never be called for IV stream.");
  auto Type = this->S->getInst()->getType();
  switch (Type->getTypeID()) {
  case llvm::Type::PointerTyID: {
    this->CurrentValue = std::stoul(DynamicVal.Value, 0, 16);
    this->IsValueValid = true;
    break;
  }
  case llvm::Type::IntegerTyID: {
    auto IntegerType = llvm::cast<llvm::IntegerType>(Type);
    unsigned BitWidth = IntegerType->getBitWidth();
    if (BitWidth > sizeof(this->CurrentValue) * 8) {
      llvm::errs() << "Too wide an integer value " << BitWidth;
      this->DEBUG_DUMP(llvm::errs());
      llvm::errs() << '\n';
    }
    assert(BitWidth <= sizeof(this->CurrentValue) * 8 &&
           "Too wide an integer value.");
    this->CurrentValue = std::stoul(DynamicVal.Value);
    this->IsValueValid = true;
    break;
  }
  default: {

    /**
     * Check if we have dependent stream.
     */
    if (!this->DependentStreams.empty()) {
      llvm_unreachable(
          "Unsupported llvm type for a load stream with dependent streams.");
    }
    return;
  }
  }

  // If we are here, it means we should update the dependent streams.
  DEBUG(llvm::errs() << "Updated loaded value ");
  DEBUG(this->DEBUG_DUMP(llvm::errs()));
  DEBUG(llvm::errs() << '\n');
  for (auto &DependentStream : this->DependentStreams) {
    DependentStream->updateRecursively(DG);
  }
}

void FunctionalStream::endStream() {
  this->HistorySerializer.serialize(this->ProtobufHistoryEntry);

  /**
   * Also dump in text format for debug purpose.
   */
  for (size_t Idx = 0, NumHistory = this->ProtobufHistoryEntry.history_size();
       Idx < NumHistory; ++Idx) {
    const auto &History = this->ProtobufHistoryEntry.history(Idx);
    this->HistoryTextFStream << History.valid() << ' ' << std::hex
                             << History.addr() << '\n';
  }
  this->HistoryTextFStream << "-----------------------\n";

  this->ProtobufHistoryEntry.clear_history();
}

void FunctionalStream::update(DataGraph *DG) {
  if (!this->BaseStreams.empty()) {
    // We should compute the value from the base stream.
    assert(this->S->Type == Stream::TypeT::MEM &&
           "Only mem stream can have base streams.");
    auto ComputedAddress = this->computeAddress(DG);
    this->IsAddressValid = ComputedAddress.first;
    this->CurrentAddress = ComputedAddress.second;
  } else {
    // This is a direct stream.
    const auto NextValue = this->Pattern.getNextValue();
    this->IsAddressValid = NextValue.first;
    this->CurrentAddress = NextValue.second;
  }

  /**
   * Set the value valid flag.
   */
  if (this->S->Type == Stream::TypeT::IV) {
    // For IV stream the value is the address.
    this->CurrentValue = this->CurrentAddress;
    this->IsValueValid = this->IsAddressValid;
  } else {
    /**
     * For Mem stream the value we will wait for the datagraph to inform us
     * the loaded value. For now we are not update the current value. This
     * means implicitly using the previous value. It may happen that the
     * loaded value is missing from the trace, in which case the value is
     * still the previous value, but we leave it as that in the histroy.
     */
    this->IsValueValid = false;
  }

  /**
   * Remember to update the history entry.
   * Note that our idx starts from 1.
   */
  assert(this->CurrentIdx != InvalidIdx &&
         "Invalid current idx to be updated.");
  if (this->ProtobufHistoryEntry.history_size() < this->CurrentIdx) {
    this->ProtobufHistoryEntry.add_history();
  }
  assert(this->ProtobufHistoryEntry.history_size() == this->CurrentIdx &&
         "Mismatch between history size and the current idx.");
  auto History =
      this->ProtobufHistoryEntry.mutable_history(this->CurrentIdx - 1);
  History->set_valid(this->IsAddressValid);
  History->set_addr(this->CurrentAddress);
}

void FunctionalStream::updateRecursively(DataGraph *DG) {

  static int StackDepth = 0;
  StackDepth++;
  if (StackDepth == 100) {
    assert(false && "Stack depth reaches 100 for updateRecursively, there "
                    "should be a circle in the stream dependence graph.");
  }

  this->update(DG);
  DEBUG(llvm::errs() << "Recursivly updated ");
  DEBUG(this->DEBUG_DUMP(llvm::errs()));
  DEBUG(llvm::errs() << '\n');
  for (auto &DependentStream : this->DependentStreams) {
    DependentStream->updateRecursively(DG);
  }

  StackDepth--;
}

std::pair<bool, uint64_t>
FunctionalStream::computeAddress(DataGraph *DG) const {

  auto MS = static_cast<MemStream *>(this->S);
  const auto FuncName = MS->getAddressFunctionName();
  const auto &AddrDG = MS->getAddressDataGraph();

  auto &Interpreter = this->SE->getInterpreter();

  auto Func = Interpreter->FindFunctionNamed(FuncName);
  assert(Func != nullptr &&
         "Failed to find the address computing function in the module.");

  bool IsValid = true;
  std::vector<llvm::GenericValue> Args;
  for (const auto &Input : AddrDG.getInputs()) {
    Args.emplace_back();

    auto BaseStreamsIter = this->BaseStreams.find(Input);
    if (BaseStreamsIter != this->BaseStreams.end()) {
      // This comes from the base stream.
      auto BaseStream = BaseStreamsIter->second;
      this->setGenericValueFromUInt64(Input->getType(), Args.back(),
                                      BaseStream->getValue());
      IsValid = IsValid && BaseStream->IsValueValid;
    } else {
      // This is an input value.
      const DynamicValue *DynamicVal =
          DG->DynamicFrameStack.front().getValueNullable(Input);

      /**
       * If somehow we failed to get the dynamic value from the datagraph, we
       * abort and mark the result invalid.
       * This is possible due to partial datagraph (I am starting to not like
       * this partial datagraph idea, but it is necessary to enable more
       * flexibility in tracing).
       */

      if (DynamicVal == nullptr) {
        llvm::errs() << "Missing dynamic value for "
                     << LoopUtils::formatLLVMValue(Input) << '\n';
        return std::make_pair(false, 0);
      }

      this->setGenericValueFromDynamicValue(Input->getType(), Args.back(),
                                            *DynamicVal);
    }
  }

  auto ResultGeneric = Interpreter->runFunction(Func, Args);
  auto Result = this->extractAddressFromGenericValue(
      AddrDG.getAddrValue()->getType(), ResultGeneric);
  return std::make_pair(IsValid, Result);
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