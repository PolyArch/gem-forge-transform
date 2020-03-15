#include "StaticMemStream.h"

#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "StaticMemStream"

void StaticMemStream::initializeMetaGraphConstruction(
    std::list<DFSNode> &DFSStack,
    ConstructedPHIMetaNodeMapT &ConstructedPHIMetaNodeMap,
    ConstructedComputeMetaNodeMapT &ConstructedComputeMetaNodeMap) {
  auto AddrValue =
      const_cast<llvm::Value *>(Utils::getMemAddrValue(this->Inst));
  const llvm::SCEV *SCEV = nullptr;
  if (this->SE->isSCEVable(AddrValue->getType())) {
    SCEV = this->SE->getSCEV(AddrValue);
  }
  this->ComputeMetaNodes.emplace_back(AddrValue, SCEV);
  ConstructedComputeMetaNodeMap.emplace(AddrValue,
                                        &this->ComputeMetaNodes.back());
  // Push to the stack.
  DFSStack.emplace_back(&this->ComputeMetaNodes.back(), AddrValue);
}

void StaticMemStream::analyzeIsCandidate() {
  /**
   * So far only look at inner most loop.
   */

  if (!this->checkBaseStreamInnerMostLoopContainsMine()) {
    this->IsCandidate = false;
    this->StaticStreamInfo.set_not_stream_reason(
        LLVM::TDG::StaticStreamInfo::
            BASE_STREAM_INNER_MOST_LOOP_NOT_CONTAIN_MINE);
    return;
  }

  /**
   * Start to check constraints:
   * 1. No PHIMetaNode.
   */
  if (!this->PHIMetaNodes.empty()) {
    this->IsCandidate = false;
    return;
  }
  /**
   * At most one StepRootStream.
   */
  if (this->BaseStepRootStreams.size() > 1) {
    this->IsCandidate = false;
    this->StaticStreamInfo.set_not_stream_reason(
        LLVM::TDG::StaticStreamInfo::MULTI_STEP_ROOT);
    return;
  }
  /**
   * ! Disable constant stream so far.
   */
  if (this->BaseStepRootStreams.size() == 0) {
    this->IsCandidate = false;
    this->StaticStreamInfo.set_not_stream_reason(
        LLVM::TDG::StaticStreamInfo::NO_STEP_ROOT);
    return;
  }
  /**
   * For store stream, ignore update streams.
   */
  if (llvm::isa<llvm::StoreInst>(this->Inst)) {
    if (!StreamPassEnableStore) {
      this->IsCandidate = false;
      return;
    }
    if (this->UpdateStream) {
      this->StaticStreamInfo.set_not_stream_reason(
          ::LLVM::TDG::StaticStreamInfo::IS_UPDATE_STORE);
      this->IsCandidate = false;
      return;
    }
  }
  assert(this->ComputeMetaNodes.size() == 1 &&
         "StaticMemStream should always have one ComputeMetaNode.");
  const auto &ComputeMNode = this->ComputeMetaNodes.front();
  auto SCEV = ComputeMNode.SCEV;
  assert(SCEV != nullptr && "StaticMemStream should always has SCEV.");
  /**
   * Validate the AddressComputeSCEV with InputSCEVs.
   */
  std::unordered_set<const llvm::SCEV *> InputSCEVs;
  for (auto &LoadBaseStream : this->LoadBaseStreams) {
    auto Inst = const_cast<llvm::Instruction *>(LoadBaseStream->Inst);
    if (!this->SE->isSCEVable(Inst->getType())) {
      this->IsCandidate = false;
      return;
    }
    InputSCEVs.insert(this->SE->getSCEV(Inst));
  }
  for (auto &IndVarBaseStream : this->IndVarBaseStreams) {
    auto Inst = const_cast<llvm::Instruction *>(IndVarBaseStream->Inst);
    if (!this->SE->isSCEVable(Inst->getType())) {
      this->IsCandidate = false;
      return;
    }
    InputSCEVs.insert(this->SE->getSCEV(Inst));
  }
  for (auto &LoopInvariantInput : this->LoopInvariantInputs) {
    if (!this->SE->isSCEVable(LoopInvariantInput->getType())) {
      this->IsCandidate = false;
      return;
    }
    InputSCEVs.insert(
        this->SE->getSCEV(const_cast<llvm::Value *>(LoopInvariantInput)));
  }
  if (this->validateSCEVAsStreamDG(SCEV, InputSCEVs)) {
    this->IsCandidate = true;
    return;
  }

  this->IsCandidate = false;
  return;
}

bool StaticMemStream::validateSCEVAsStreamDG(
    const llvm::SCEV *SCEV,
    const std::unordered_set<const llvm::SCEV *> &InputSCEVs) {
  while (true) {
    if (InputSCEVs.count(SCEV) != 0) {
      break;
    } else if (llvm::isa<llvm::SCEVAddRecExpr>(SCEV)) {
      break;
    } else if (auto ComSCEV = llvm::dyn_cast<llvm::SCEVCommutativeExpr>(SCEV)) {
      const llvm::SCEV *LoopVariantSCEV = nullptr;
      for (size_t OperandIdx = 0, NumOperands = ComSCEV->getNumOperands();
           OperandIdx < NumOperands; ++OperandIdx) {
        auto OpSCEV = ComSCEV->getOperand(OperandIdx);
        if (this->SE->isLoopInvariant(OpSCEV, this->ConfigureLoop)) {
          // Loop invariant.
          continue;
        }
        if (InputSCEVs.count(OpSCEV)) {
          // Input SCEV.
          continue;
        }
        if (llvm::isa<llvm::SCEVAddRecExpr>(OpSCEV)) {
          // AddRec SCEV.
          continue;
        }
        if (LoopVariantSCEV == nullptr) {
          LoopVariantSCEV = OpSCEV;
        } else {
          // More than one LoopVariant/Non-Input SCEV operand.
          return false;
        }
      }
      if (LoopVariantSCEV == nullptr) {
        // We are done.
        break;
      }
      SCEV = LoopVariantSCEV;
    } else if (auto CastSCEV = llvm::dyn_cast<llvm::SCEVCastExpr>(SCEV)) {
      SCEV = CastSCEV->getOperand();
    } else {
      return false;
    }
  }
  return true;
}

bool StaticMemStream::checkIsQualifiedWithoutBackEdgeDep() const {
  if (!this->isCandidate()) {
    return false;
  }
  // Check all the base streams are qualified.
  for (auto &BaseStream : this->BaseStreams) {
    if (!BaseStream->isQualified()) {
      this->StaticStreamInfo.set_not_stream_reason(
          LLVM::TDG::StaticStreamInfo::BASE_STREAM_NOT_QUALIFIED);
      return false;
    }
  }
  // All the base streams are qualified, and thus know their StepPattern. We can
  // check the constraints on static mapping.
  if (!this->checkStaticMapFromBaseStreamInParentLoop()) {
    this->StaticStreamInfo.set_not_stream_reason(
        LLVM::TDG::StaticStreamInfo::NO_STATIC_MAPPING);
    LLVM_DEBUG(llvm::errs()
               << "[UnQualify]: NoStaticMap " << this->formatName() << '\n');
    return false;
  }
  return true;
}

bool StaticMemStream::checkIsQualifiedWithBackEdgeDep() const {
  // MemStream should have no back edge dependence.
  assert(this->BackMemBaseStreams.empty() &&
         "Memory stream should not have back edge base stream.");
  return this->checkIsQualifiedWithoutBackEdgeDep();
}

LLVM::TDG::StreamStepPattern StaticMemStream::computeStepPattern() const {
  // Copy the step pattern from the step root.
  if (this->BaseStepRootStreams.empty()) {
    return LLVM::TDG::StreamStepPattern::NEVER;
  } else {
    assert(this->BaseStepRootStreams.size() == 1 &&
           "More than one step root stream to finalize pattern.");
    auto StepRootStream = *(this->BaseStepRootStreams.begin());
    auto StepRootStreamStpPattern =
        StepRootStream->StaticStreamInfo.stp_pattern();
    LLVM_DEBUG(llvm::errs()
               << "Computed step pattern " << StepRootStreamStpPattern
               << " for " << this->formatName() << '\n');
    return StepRootStreamStpPattern;
  }
}

void StaticMemStream::finalizePattern() {
  this->StaticStreamInfo.set_stp_pattern(this->computeStepPattern());
  // Compute the value pattern.
  this->StaticStreamInfo.set_val_pattern(
      LLVM::TDG::StreamValuePattern::CONSTANT);
  for (auto &BaseStream : this->BaseStreams) {
    if (BaseStream->Type == MEM) {
      // This makes me indirect stream.
      this->StaticStreamInfo.set_val_pattern(
          LLVM::TDG::StreamValuePattern::INDIRECT);
      break;
    } else {
      // This is likely our step root, copy its value pattern.
      this->StaticStreamInfo.set_val_pattern(
          BaseStream->StaticStreamInfo.val_pattern());
    }
  }
  // Compute the possible loop path.
  this->StaticStreamInfo.set_loop_possible_path(
      LoopUtils::countPossiblePath(this->InnerMostLoop));
  this->StaticStreamInfo.set_config_loop_possible_path(
      LoopUtils::countPossiblePath(this->ConfigureLoop));

  /**
   * Manually set float decision for some workloads.
   */
  const auto &DebugLoc = this->Inst->getDebugLoc();
  if (!DebugLoc) {
    /**
     * It's possible that the load has no location, e.g.
     * the compiler converts a[i - 1] into a PHINode.
     */
    return;
  }
  assert(DebugLoc && "Missing DebugLoc for MemStream.");
  auto Line = DebugLoc.getLine();
  auto Scope = llvm::dyn_cast<llvm::DIScope>(DebugLoc.getScope());
  auto SourceFile = Scope->getFilename();

  if (SourceFile == "hotspot.cpp") {
    // rodinia.hotspot, power[], no reuse.
    if (Line == 81) {
      this->StaticStreamInfo.set_float_manual(true);
      // } else if (Line >= 82 && Line <= 86) {
      //   // rodinia.hotspot, temp[], reuse 8kB.
      //   this->StaticStreamInfo.set_float_manual(true);
    }
  } else if (SourceFile == "pathfinder.cpp" && Line == 91) {
    // rodinia.pathfinder, wall[], no reuse.
    this->StaticStreamInfo.set_float_manual(true);
  } else if (SourceFile == "3D.c") {
    if (Line == 176 || Line == 177) {
      // rodinia.hotspot3D, tIn_t[top/bottom]
      this->StaticStreamInfo.set_float_manual(true);
    } else if (Line == 170) {
      // rodinia.hotspot3D, pIn_t[c]
      this->StaticStreamInfo.set_float_manual(true);
    }
  } else if (SourceFile == "srad.cpp") {
    if (Line >= 214 && Line <= 217) {
      // rodinia.srad_v2, delta[], no reuse.
      this->StaticStreamInfo.set_float_manual(true);
      // } else if (Line >= 208 && Line <= 211) {
      //   // rodinia.srad_v2, c[] reuse in MLC.
      //   this->StaticStreamInfo.set_float_manual(true);
      // } else if (Line >= 151 && Line <= 155) {
      //   // rodinia.srad_v2, J[] reuse in MLC.
      //   this->StaticStreamInfo.set_float_manual(true);
    }
  } else if (SourceFile == "needle.cpp") {
    if (Line == 106 || Line == 142) {
      // rodinia.nw, reference[], no reuse.
      this->StaticStreamInfo.set_float_manual(true);
    }
  } else if (SourceFile == "kmeans_clustering.c") {
    // rodinia.kmeans, No stream for floating.
  } else if (SourceFile == "euler3d_cpu_double.cpp") {
    // rodinia.cfd.
    if (Line == 52) {
      // src[] in copy(), no reuse.
      this->StaticStreamInfo.set_float_manual(true);
    } else if (Line >= 160 && Line <= 164) {
      // variables[], 471kB, no reuse.
      this->StaticStreamInfo.set_float_manual(true);
    } else if (Line == 175) {
      // area[], should we float it?
      this->StaticStreamInfo.set_float_manual(true);
    } else if (Line >= 199 && Line <= 205) {
      // variables, no reuse.
      this->StaticStreamInfo.set_float_manual(true);
    } else if (Line >= 238 && Line <= 242) {
      // elements_surrounding_elements[], normals[], too large.
      this->StaticStreamInfo.set_float_manual(true);
    } else if (Line >= 248 && Line <= 252) {
      // indirect access to variables[], so far not float.
      // This is interesting.
    } else if (Line == 378) {
      // step_factors[], float.
      this->StaticStreamInfo.set_float_manual(true);
    } else if (Line >= 387 && Line <= 391) {
      // old_variables, 471kB, no reuse.
      this->StaticStreamInfo.set_float_manual(true);
    } else if (Line >= 393 && Line <= 397) {
      // fluxes, 471kB, no reuse.
      this->StaticStreamInfo.set_float_manual(true);
    }
  } else if (SourceFile == "hotspot_kernel.c") {
    if (Line == 55) {
      // a[]
      this->StaticStreamInfo.set_float_manual(true);
    }
  } else if (SourceFile == "bfs.cpp") {
    // rodinia.bfs
    if (Line == 37) {
      // masks[]
      this->StaticStreamInfo.set_float_manual(true);
    } else if (Line == 64) {
      // updates[]
      this->StaticStreamInfo.set_float_manual(true);
    } else if (Line == 44) {
      // edges[]
      if (StreamPassMergeIndPredicatedStore) {
        // Only offload this if we merge the predicated stores.
        this->StaticStreamInfo.set_float_manual(true);
      }
    }
  } else if (SourceFile == "kernel_query.c") {
    // rodinia.b+tree
    if (Line == 64) {
      // Leaf key[].
      this->StaticStreamInfo.set_float_manual(true);
    }
  } else if (SourceFile == "kernel_range.c") {
    // rodinia.b+tree
    if (Line == 86 || Line == 89) {
      // Leaf key[].
      this->StaticStreamInfo.set_float_manual(true);
    }
  } else if (SourceFile == "nn.c") {
    // rodinia.nn
    if (Line == 191 || Line == 192) {
      // sandbox[], no reuse at all.
      this->StaticStreamInfo.set_float_manual(true);
    } else if (Line == 223) {
      // z[], no reuse at all.
      this->StaticStreamInfo.set_float_manual(true);
    }
  } else if (SourceFile == "particlefilter.c") {
    // rodinia.particlefilter
    if (Line == 298) {
      // CDF[], no reuse at all.
      this->StaticStreamInfo.set_float_manual(true);
    }
  }
}
