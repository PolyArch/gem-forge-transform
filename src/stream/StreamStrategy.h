#ifndef GEM_FORGE_STREAM_STRATEGY_H
#define GEM_FORGE_STREAM_STRATEGY_H

/**
 * Defines various strategies used in stream selection.
 *
 * Stream selection is divided into these phases.
 * 1. Candidate.
 *    This performs some basic static checking.
 *    Implemented by Stream::isCandidate().
 * 2. QualifySeed.
 *    This stream can be used as the seed to propagate quailify signal.
 *    For IV streams, back-edge dependence is ignored in this phase.
 *    Implemented by Stream::isQualifySeed().
 * 3. Enforce back-edge dependence.
 *    To support pointer chasing patter, we ignore back-edge dependence in
 *    previous phase. Now we enforce it.
 * 4. Choose stream.
 *    This phase we pick the configure loop level. Actually this is
 *    problematic, cause it gives freedom on configuring streams at outer
 *    loop.
 */

enum StreamPassQualifySeedStrategyE {
  STATIC,
  DYNAMIC,
};

enum StreamPassChooseStrategyE {
  DYNAMIC_OUTER_MOST,
  STATIC_OUTER_MOST,
  INNER_MOST
};

#endif