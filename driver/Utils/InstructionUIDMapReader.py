
class InstructionDescriptor(object):
    def __init__(self, op_name, func_name, bb_name, pos_in_bb):
        self.op_name = op_name
        self.func_name = func_name
        self.bb_name = bb_name
        self.pos_in_bb = pos_in_bb


class InstructionUIDMapReader(object):
    def __init__(self, fn):
        self.uid_inst_map = dict()
        self.func_set = set()
        with open(fn) as f:
            for line in f:
                fields = line.split()
                uid = int(fields[0])
                func_name = fields[1]
                bb_name = fields[2]
                pos_in_bb = fields[3]
                op_name = fields[4]
                descriptor = InstructionDescriptor(
                    op_name=op_name,
                    func_name=func_name,
                    bb_name=bb_name,
                    pos_in_bb=pos_in_bb
                )
                self.uid_inst_map[uid] = descriptor
                self.func_set.add(func_name)

    def hasFunction(self, func_name):
        return func_name in self.func_set
