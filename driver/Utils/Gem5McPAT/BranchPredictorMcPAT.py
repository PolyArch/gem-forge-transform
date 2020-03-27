
import math

def configureBranchPredictor(bp, mcpatCore):
    mcpatCore.RAS_size = bp.RASSize

    # Branch target buffer.
    mcpatBTB = mcpatCore.BTB
    mcpatBTB.BTB_config[0] = 6
    mcpatBTB.BTB_config[1] = bp.BTBEntries
    mcpatBTB.BTB_config[2] = 6
    mcpatBTB.BTB_config[3] = 2
    mcpatBTB.BTB_config[4] = 1
    mcpatBTB.BTB_config[5] = 1

    mcpatPredictor = mcpatCore.predictor
    bpType = bp.type
    if bpType == 'LocalBP':
        mcpatPredictor.local_predictor_entries = bp.localPredictorSize
        mcpatPredictor.local_predictor_size[0] = math.floor(math.log(bp.localPredictorSize, 2))
        mcpatPredictor.local_predictor_size[1] = bp.localCtrBits

    elif bpType == 'TournamentBP':
        mcpatPredictor.local_predictor_entries = bp.localPredictorSize
        mcpatPredictor.local_predictor_size[0] = math.floor(math.log(bp.localPredictorSize, 2))
        mcpatPredictor.local_predictor_size[1] = bp.localCtrBits

        mcpatPredictor.global_predictor_entries = bp.globalPredictorSize
        mcpatPredictor.global_predictor_bits = bp.globalCtrBits

        mcpatPredictor.chooser_predictor_entries = bp.choicePredictorSize
        mcpatPredictor.chooser_predictor_bits = bp.choiceCtrBits

    elif bpType == 'LTAGE':
        # ! Not sure how this work.
        print('Warn! Not sure of correctly supporting predictor {s}.'.format(s=bpType))

        # Partially tagged tabe is modeled as local predictor.
        tage = bp.tage
        nHistoryTables = tage['nHistoryTables']
        logTagTableSizes = tage['logTagTableSizes']
        tagTableTagWidths = tage['tagTableTagWidths']
        tagTableTotalBits = 0
        tagTableTotalEntries = 0
        for i in range(len(logTagTableSizes)):
            tagBits = tagTableTagWidths[i]
            size = 2 ** logTagTableSizes[i]
            tagTableTotalEntries += size
            # Add 1 bit for branch outcome.
            tagTableTotalBits += (tagBits + 1) * size

        # HistoryBuffer is modeld as global predictor.
        historyBufferSize = tage['histBufferSize']

        mcpatPredictor.local_predictor_entries = tagTableTotalEntries
        mcpatPredictor.local_predictor_size[0] = math.floor(math.log(tagTableTotalEntries, 2))
        mcpatPredictor.local_predictor_size[1] = int(tagTableTotalBits / tagTableTotalEntries)

        mcpatPredictor.global_predictor_entries = historyBufferSize
        mcpatPredictor.global_predictor_bits = 1 # Not sure.

        # Not sure.
        # mcpatPredictor.chooser_predictor_entries = bp.choicePredictorSize
        # mcpatPredictor.chooser_predictor_bits = bp.choiceCtrBits

    else:
        print('Warn! Unsupported type of predictor {s}.'.format(s=bpType))

def setStatsBranchPredictor(self, bp, mcpatCore):
    def scalar(stat): return self.getScalarStats(bp.path + '.' + stat)
    bpType = bp.type

    reads = scalar("BTBLookups")

    mcpatBTB = mcpatCore.BTB
    mcpatBTB.read_accesses = reads
    # Gem5 seems missing the stats.
    mcpatBTB.write_accesses = 0

    predictorAccesses = scalar("lookups")
    mcpatPredictor = mcpatCore.predictor
    mcpatPredictor.predictor_accesses = predictorAccesses