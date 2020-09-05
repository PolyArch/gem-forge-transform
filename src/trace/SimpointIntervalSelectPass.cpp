#include "SimpointIntervalSelectPass.h"
#include "../Utils.h"
#include "ProfileLogger.h"

// Reuse the -trace-file Option.
#include "../DataGraph.h"

SimpointIntervalSelectPass::SimpointIntervalSelectPass()
    : GemForgeBasePass(ID) {}

SimpointIntervalSelectPass::~SimpointIntervalSelectPass() {}

bool SimpointIntervalSelectPass::runOnModule(llvm::Module &Module) {
  this->initialize(Module);

  // Initialize the tree.
  this->CLProfileTree = new CallLoopProfileTree(this->CachedLI);

  // Initialize the reader.
  this->Reader = new GzipMultipleProtobufReader(TraceFileName);
  uint64_t UID;
  while (this->Reader->readVarint64(UID)) {
    auto Inst = Utils::getInstUIDMap().getInst(UID);
    this->CLProfileTree->addInstruction(Inst);
  }
  delete this->Reader;
  this->Reader = nullptr;

  this->CLProfileTree->aggregateStats();

  const auto &Points = this->CLProfileTree->selectEdges();
  {
    this->Reader = new GzipMultipleProtobufReader(TraceFileName);
    ProfileLogger Logger;

    uint64_t UID;
    // Search for the first interval point.
    uint64_t IntervalLHSInstCount = 0;
    uint64_t IntervalRHSInstCount = 0;
    uint64_t IntervalLHSMarkCount = 0;
    uint64_t IntervalRHSMarkCount = 0;
    auto PointIter = Points.begin();
    auto PointEnd = Points.end();
    while (PointIter != PointEnd) {
      if (PointIter->Selected) {
        IntervalLHSInstCount = PointIter->TraverseInstCount;
        IntervalRHSInstCount = PointIter->TraverseInstCount;
        IntervalLHSMarkCount = PointIter->TraverseMarkCount;
        IntervalRHSMarkCount = PointIter->TraverseMarkCount;
        PointIter++;
        break;
      }
      PointIter++;
    }
    while (this->Reader->readVarint64(UID)) {
      auto InstCount = Logger.getCurrentInstCount();
      if (InstCount >= IntervalRHSInstCount) {
        if (IntervalRHSInstCount > IntervalLHSInstCount) {
          // We have a valid interval.
          assert(Logger.getCurrentIntervalLHS() == IntervalLHSInstCount);
          Logger.saveAndRestartInterval(IntervalRHSMarkCount);
        } else {
          // This is the first invalid interval.
          Logger.discardAndRestartInterval(IntervalRHSMarkCount);
        }
        IntervalLHSInstCount = IntervalRHSInstCount;
        IntervalLHSMarkCount = IntervalRHSMarkCount;
        // Search for next RHSInterval.
        while (PointIter != PointEnd) {
          if (PointIter->Selected) {
            IntervalRHSInstCount = PointIter->TraverseInstCount;
            IntervalRHSMarkCount = PointIter->TraverseMarkCount;
            PointIter++;
            break;
          }
          PointIter++;
        }
      }
      auto Inst = Utils::getInstUIDMap().getInst(UID);
      Logger.addInst(Inst->getFunction()->getName(),
                     Inst->getParent()->getName());
    }
    delete this->Reader;
    this->Reader = nullptr;
    std::string ProfileFileName = TraceFileName + ".profile";
    Logger.serializeToFile(ProfileFileName);
  }

  std::string TreeFileName = TraceFileName + ".tree.txt";
  std::ofstream OTree(TreeFileName);
  this->CLProfileTree->dump(OTree);
  OTree.close();

  // Do the trasformation only once.
  //   this->transform();

  //   std::string Stats = this->OutTraceName + ".stats.txt";
  //   LLVM_DEBUG(llvm::errs() << "Dump stats to " << Stats << '\n');
  //   std::ofstream OStats(Stats);
  //   this->dumpStats(OStats);
  //   OStats.close();

  //   // Dump the cache warmer record.
  //   this->CacheWarmerPtr->dumpToFile();

  //   bool Modified = false;
  //   if (this->Trace->DetailLevel == DataGraph::DataGraphDetailLv::INTEGRATED)
  //   {
  //     for (auto FuncIter = Module.begin(), FuncEnd = Module.end();
  //          FuncIter != FuncEnd; ++FuncIter) {
  //       llvm::Function &Function = *FuncIter;
  //       Modified |= this->processFunction(Function);
  //     }
  //   }

  delete this->CLProfileTree;
  this->CLProfileTree = nullptr;

  this->finalize(Module);

  return false;
}

char SimpointIntervalSelectPass::ID = 0;
static llvm::RegisterPass<SimpointIntervalSelectPass>
    R("simpoint-interval", "Select simpoint interval from trace", false, false);