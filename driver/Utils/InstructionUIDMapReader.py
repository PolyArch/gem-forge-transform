import UIDMap_pb2


class InstructionUIDMapReader(object):
    def __init__(self, fn):
        self.uid_inst_map = dict()
        self.func_set = set()
        with open(fn) as f:
            content = f.read()
            self.uid_inst_map = UIDMap_pb2.UIDMap()
            self.uid_inst_map.ParseFromString(content)
        for uid in self.uid_inst_map.inst_map:
            descriptor = self.uid_inst_map.inst_map[uid]
            self.func_set.add(descriptor.func)

    def hasFunction(self, func_name):
        return func_name in self.func_set

    def getDescriptor(self, uid):
        return self.uid_inst_map.inst_map[uid]
