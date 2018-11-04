#include "StreamPassTestFixture.h"

class StreamTransformPassStreamInstDependenceTestFixture
    : public StreamTransformPassTestFixture {
protected:
  using ExpectedInstDependenceMapT =
      std::map<DynamicInstruction::DynamicId,
               std::unordered_set<DynamicInstruction::DynamicId>>;

  void testInstDependence(
      const std::string &OutputDataGraphFile,
      const ExpectedInstDependenceMapT &ExpectedInstDependenceMap) {
    Gem5ProtobufReader Reader(OutputDataGraphFile);
    TextProtobufSerializer TextSerializer(OutputDataGraphFile +
                                          ".serialized.txt");

    LLVM::TDG::TDGInstruction Inst;
    LLVM::TDG::StaticInformation StaticInfo;

    // First read and discard the StaticInfo.
    ASSERT_TRUE(Reader.read(StaticInfo))
        << "Failed to read the static information.";

    // Remap the DynamicInstructionId back to sequencial space so that
    // we can easily compare with the expected dependence.
    std::unordered_map<DynamicInstruction::DynamicId,
                       DynamicInstruction::DynamicId>
        DynamicIdToSequenceMap;

    DynamicInstruction::DynamicId CurrentSeq = 0;
    std::unordered_set<DynamicInstruction::DynamicId> ActualDeps;
    while (Reader.read(Inst)) {
      // Update the map.
      DynamicIdToSequenceMap.emplace(Inst.id(), CurrentSeq);

      // Translate the dependence back to sequence id.
      ActualDeps.clear();

      // We only care about register dependence.
      for (const auto &Dep : Inst.reg_deps()) {
        ASSERT_TRUE(DynamicIdToSequenceMap.count(Dep) != 0)
            << "Failed to translate the dynamic id " << Dep;
        ActualDeps.insert(DynamicIdToSequenceMap.at(Dep));
      }

      auto ExpectedIter = ExpectedInstDependenceMap.find(CurrentSeq);
      if (ExpectedIter != ExpectedInstDependenceMap.end()) {
        // Check the dependence.
        const auto &ExpectedDeps = ExpectedIter->second;

        this->testTwoSets(ExpectedDeps, ActualDeps);
        // this->testASubsetOfB(ExpectedDeps, ActualDeps);
      }

      // Let's dump the translated datagraph to a text file for human read and
      // easily debug.
      Inst.clear_reg_deps();
      for (const auto &Dep : ActualDeps) {
        Inst.add_reg_deps(Dep);
      }
      Inst.set_id(CurrentSeq);
      TextSerializer.serialize(Inst);

      // Addvance the sequence.
      CurrentSeq++;
    }
  }
};

TEST_F(StreamTransformPassStreamInstDependenceTestFixture,
       BasicListTransverse) {
  this->setUpEnvironment(
      GetTestInputSource("/stream/inputs/BasicListTransverse.c"));

  this->Pass.runOnModule(*(this->Module));

  ExpectedInstDependenceMapT Expected = {
      {7, {}},          // stream-config.
      {10, {7, 9}},     // stream-store.
      {17, {10, 16}},   // stream-store.
      {24, {17, 23}},   // stream-store.
      {31, {24, 30}},   // stream-store.
      {43, {31}},       // stream-config
      {46, {43, 45}},   // stream-store.
      {53, {46, 52}},   // stream-store.
      {60, {53, 59}},   // stream-store.
      {67, {60, 66}},   // stream-store.
      {79, {67}},       // stream-config
      {82, {79, 81}},   // stream-store.
      {89, {82, 88}},   // stream-store.
      {96, {89, 95}},   // stream-store.
      {103, {96, 102}}, // stream-store.
  };
  this->testInstDependence(this->Input->getOutputDataGraphFile(), Expected);
}

TEST_F(StreamTransformPassStreamInstDependenceTestFixture, StoreToSamePlace) {
  this->setUpEnvironment(
      GetTestInputSource("/stream/inputs/StoreToSamePlace.c"));

  this->Pass.runOnModule(*(this->Module));

  ExpectedInstDependenceMapT Expected = {
      {7, {}},     // stream-config.
      {10, {7}},   // stream-store.
      {17, {7}},   // stream-store.
      {24, {7}},   // stream-store.
      {31, {7}},   // stream-store.
      {43, {7}},   // stream-config
      {46, {43}},  // stream-store.
      {53, {43}},  // stream-store.
      {60, {43}},  // stream-store.
      {67, {43}},  // stream-store.
      {79, {43}},  // stream-config
      {82, {79}},  // stream-store.
      {89, {79}},  // stream-store.
      {96, {79}},  // stream-store.
      {103, {79}}, // stream-store.
  };
  //   this->testInstDependence(this->Input->getOutputDataGraphFile(),
  //   Expected);
}