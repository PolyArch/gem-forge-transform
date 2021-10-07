#include "FunctionalStream.h"
#include "FunctionalStreamEngine.h"

#include "llvm/Support/Format.h"

#include "google/protobuf/util/json_util.h"

#define DEBUG_TYPE "FunctionalStream"
#if !defined(LLVM_DEBUG) && defined(DEBUG)
#define LLVM_DEBUG DEBUG
#endif

FunctionalStream::FunctionalStream(DynStream *_S, FunctionalStreamEngine *_SE)
    : S(_S), SE(_SE), Pattern(_S->getProtobufPatterns()),
      CurrentIdx(InvalidIdx), IsAddressValid(false), IsValueValid(false) {
  LLVM_DEBUG(llvm::dbgs() << "Initialized FunctionalStream of "
                          << S->getStreamName() << '\n');
}

void FunctionalStream::addBaseStream(FunctionalStream *BaseStream) {
  assert(BaseStream != this && "No self dependence.");
  this->BaseStreams.emplace(BaseStream->getStream()->getInst(), BaseStream);
  BaseStream->DependentStreams.insert(this);
}

void FunctionalStream::DEBUG_DUMP(llvm::raw_ostream &OS) const {
  OS << this->S->getStreamName() << " idx " << this->CurrentIdx << " addr "
     << llvm::format_hex(this->CurrentAddress, 18) << " value "
     << llvm::format_hex(this->CurrentValue, 18) << " valid "
     << this->IsValueValid;
}

void FunctionalStream::configure(DataGraph *DG) {
  this->ProtobufHistoryEntries.emplace_back();
  this->CurrentProtobufHistory = &(this->ProtobufHistoryEntries.back());
  this->CurrentProtobufHistory->set_id(this->S->getStreamId());

  this->Pattern.configure();
  this->CurrentIdx = InvalidIdx;
  this->CurrentIdx++;
  this->CurrentEntryUsed = false;
  this->update(DG);
  LLVM_DEBUG(llvm::dbgs() << "Configured ");
  LLVM_DEBUG(this->DEBUG_DUMP(llvm::errs()));
  LLVM_DEBUG(llvm::dbgs() << '\n');
}

void FunctionalStream::updateHistory() {
  /**
   * Remember to update the history entry before we step.
   * Note that our idx starts from 1.
   */
  assert(this->CurrentIdx != InvalidIdx &&
         "Invalid current idx to be updated.");
  if (this->CurrentProtobufHistory->history_size() < this->CurrentIdx) {
    this->CurrentProtobufHistory->add_history();
  }
  assert(this->CurrentProtobufHistory->history_size() == this->CurrentIdx &&
         "Mismatch between history size and the current idx.");
  auto History =
      this->CurrentProtobufHistory->mutable_history(this->CurrentIdx - 1);
  History->set_valid(this->IsAddressValid);
  History->set_addr(this->CurrentAddress);
  History->set_used(this->CurrentEntryUsed);
}

void FunctionalStream::step(DataGraph *DG) {
  static int StackDepth = 0;
  StackDepth++;

  this->updateHistory();

  /**
   * Add the memory footprint.
   */
  if (this->S->SStream->Type == StaticStream::TypeT::MEM) {
    this->MemFootprint.access(this->CurrentAddress);
  }

  this->CurrentIdx++;
  this->CurrentEntryUsed = false;
  this->Pattern.step();
  this->update(DG);
}

void FunctionalStream::access() {
  assert(this->CurrentIdx != InvalidIdx &&
         "Access when there is no valid entry.");
  this->CurrentEntryUsed = true;
}

void FunctionalStream::setValue(const DynamicValue &DynamicVal) {
  auto Type = this->S->getInst()->getType();
  switch (Type->getTypeID()) {
  case llvm::Type::PointerTyID: {
    this->CurrentValue = DynamicVal.getAddr();
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
    this->CurrentValue = DynamicVal.getInt();
    this->IsValueValid = true;
    break;
  }
  default: {
    /**
     * Check if we have dependent stream.
     */
    if (!this->DependentStreams.empty()) {
      llvm_unreachable(
          "Unsupported llvm type for a stream with dependent streams.");
    }
    return;
  }
  }
}

void FunctionalStream::endStream() {

  // We need to record the last element value.
  this->updateHistory();

  // Add footprint information.
  if (this->S->SStream->Type == StaticStream::TypeT::MEM) {
    this->CurrentProtobufHistory->set_num_cache_lines(
        this->MemFootprint.getNumCacheLinesAccessed());
    this->CurrentProtobufHistory->set_num_accesses(
        this->MemFootprint.getNumAccesses());
    this->MemFootprint.clear();
  }
}

void FunctionalStream::endAll() {
  Gem5ProtobufSerializer HistorySerializer(this->S->getHistoryFullPath());
  std::ofstream HistoryTextFStream(
      this->S->getTextPath(this->S->getHistoryFullPath()));
  assert(HistoryTextFStream.is_open() &&
         "Failed to open the history text ofstream.");
  for (auto &H : this->ProtobufHistoryEntries) {
    HistorySerializer.serialize(H);

    std::string JSONString;
    google::protobuf::util::MessageToJsonString(H, &JSONString);
    HistoryTextFStream << JSONString << '\n';
  }
  HistoryTextFStream.close();
}

void FunctionalStream::update(DataGraph *DG) {
  if (!this->BaseStreams.empty()) {
    // We should compute the value from the base stream.
    assert(this->S->SStream->Type == StaticStream::TypeT::MEM &&
           "Only mem stream can have base streams.");
    auto ComputedAddress = this->computeAddress(DG);
    this->IsAddressValid = ComputedAddress.first;
    this->CurrentAddress = ComputedAddress.second;
  } else {
    // This is a direct stream.
    const auto Value = this->Pattern.getValue();
    this->IsAddressValid = Value.first;
    this->CurrentAddress = Value.second;
  }

  /**
   * Set the value valid flag.
   */
  if (this->S->SStream->Type == StaticStream::TypeT::IV) {
    // For IV stream the value is the address.
    this->CurrentValue = this->CurrentAddress;
    this->IsValueValid = this->IsAddressValid;
  }
}

std::pair<bool, uint64_t>
FunctionalStream::computeAddress(DataGraph *DG) const {
  auto MS = static_cast<DynMemStream *>(this->S);
  const auto FuncName = MS->getAddressFunctionName();
  const auto &AddrDG = MS->getAddrDG();

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
      if (llvm::isa<llvm::GlobalVariable>(Input)) {
        auto GlobalValueEnvIter = DG->GlobalValueEnv.find(Input);
        if (GlobalValueEnvIter != DG->GlobalValueEnv.end()) {
          DynamicVal = &(GlobalValueEnvIter->second);
        }
      }

      /**
       * If somehow we failed to get the dynamic value from the datagraph, we
       * abort and mark the result invalid.
       * This is possible due to partial datagraph (I am starting to not like
       * this partial datagraph idea, but it is necessary to enable more
       * flexibility in tracing).
       */

      if (DynamicVal == nullptr) {
        this->DEBUG_DUMP(llvm::errs());
        llvm::errs() << " Missing dynamic value for "
                     << Utils::formatLLVMValue(Input) << '\n';
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
    GenericVal.PointerVal = reinterpret_cast<void *>(DynamicVal.getAddr());
    break;
  }
  case llvm::Type::IntegerTyID: {
    auto IntegerType = llvm::cast<llvm::IntegerType>(Type);
    unsigned BitWidth = IntegerType->getBitWidth();
    uint64_t Val = DynamicVal.getInt();
    GenericVal.IntVal = llvm::APInt(BitWidth, Val);
    break;
  }
  case llvm::Type::DoubleTyID: {
    GenericVal.DoubleVal = DynamicVal.getDouble();
    break;
  }
  default: {
    llvm::errs() << "Unsupported llvm type found for " << this->S->getStreamName()
                 << '\n';
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