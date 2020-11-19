#ifndef GEM_FORGE_STREAM_EXECUTION_PASS_H
#define GEM_FORGE_STREAM_EXECUTION_PASS_H

#include "stream/StreamPass.h"

/**
 * This pass will actually clone the module and transform it using identified
 * stream. This is used for execution-driven simulation.
 */

class StreamExecutionPass : public StreamPass {
public:
  static char ID;
  StreamExecutionPass() : StreamPass(ID) {}

protected:
  void transformStream() override;
  /**
   * Since we are purely static trasnformation, we have to first select
   * DynStreamRegionAnalyzer as our candidate. Here I sort them by the number of
   * dynamic memory accesses and exclude overlapped regions.
   */
  std::vector<DynStreamRegionAnalyzer *> selectStreamRegionAnalyzers();
};

#endif
