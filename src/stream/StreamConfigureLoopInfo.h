#ifndef GEM_FORGE_STREAM_CONFIG_LOOP_INFO_H
#define GEM_FORGE_STREAM_CONFIG_LOOP_INFO_H

#include "stream/DynStream.h"

class StreamConfigureLoopInfo {
public:
  StreamConfigureLoopInfo(const std::string &_Folder,
                          const std::string &_RelativeFolder,
                          const llvm::Loop *_Loop,
                          std::vector<StaticStream *> _SortedStreams);

  const std::string &getRelativePath() const { return this->RelativePath; }
  const llvm::Loop *getLoop() const { return this->Loop; }
  const std::vector<StaticStream *> &getSortedStreams() const {
    return this->SortedStreams;
  }

  /**
   * TotalConfiguredStreams: Sum of all configured streams in this loop and
   * parent loops.
   * TotalSubLoopStreams: Maximum number of possible streams configured in
   * this loop and sub-loops.
   * TotalPeerStreams: Maximum number of possible streams be alive the
   * same time with streams configured in this loop.
   *
   * Formula:
   * * TotalConfiguredStreams = Sum(ConfiguredStreams, ThisAndParentLoop)
   * * TotalSubLoopStreams = \
   * *   (Max(TotalSubLoopStreams, SubLoop) or 0) + ConfiguredStreams
   * * TotalAliveStreams = TotalConfiguredStreams + \
   * *   TotalSubLoopStreams - COnfiguredStreams
   */
  int TotalConfiguredStreams;
  int TotalConfiguredCoalescedStreams;
  int TotalSubLoopStreams;
  int TotalSubLoopCoalescedStreams;
  int TotalAliveStreams;
  int TotalAliveCoalescedStreams;
  std::vector<StaticStream *> SortedCoalescedStreams;

  void dump(llvm::DataLayout *DataLayout) const;

  void addNestConfigureInfo(
      StreamConfigureLoopInfo *NestConfigureInfo,
      std::unique_ptr<::LLVM::TDG::ExecFuncInfo> NestConfigureFuncInfo,
      std::unique_ptr<::LLVM::TDG::ExecFuncInfo> PredFuncInfo,
      bool PredicateRet);

  void addLoopBoundDG(BBBranchDataGraph *LoopBoundDG);
  BBBranchDataGraph *getLoopBoundDG() const { return this->LoopBoundDG; }

private:
  const std::string Path;
  const std::string RelativePath;
  const std::string JsonPath;
  const llvm::Loop *Loop;
  std::vector<StaticStream *> SortedStreams;

  /**
   * Remember the nest stream region.
   */
  std::vector<StreamConfigureLoopInfo *> NestConfigureInfos;
  std::unique_ptr<::LLVM::TDG::ExecFuncInfo> NestConfigureFuncInfo;
  std::unique_ptr<::LLVM::TDG::ExecFuncInfo> NestPredFuncInfo;
  bool NestPredRet;

  /**
   * Remember the LoopBoundDG.
   */
  BBBranchDataGraph *LoopBoundDG = nullptr;
};

#endif