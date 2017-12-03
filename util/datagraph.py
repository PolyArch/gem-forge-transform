import sys


def debug(msg):
    print(msg)


class StaticInst:
    def __init__(self):
        self.func_name = ''
        self.bb_name = ''
        self.static_id = -1
        self.op_name = ''
        self.result = ''
        self.operands = list()


class BasicBlock:
    def __init__(self, name):
        self.name = name
        self.insts = {}


class Function:
    def __init__(self, name):
        self.name = name
        self.bbs = dict()
        # A map from LLVM value to the static inst.
        self.value_inst_map = dict()


class DynamicValue:
    def __init__(self):
        self.name = ''
        self.type_name = ''
        self.type_id = -1
        self.value = ''


class DynamicInst:
    def __init__(self, dynamic_id, static_inst):
        self.dynamic_id = dynamic_id
        self.static_inst = static_inst
        self.deps = list()
        self.dynamic_values = list()
        self.dynamic_result = None


class DataGraph:
    def __init__(self):
        self.functions = dict()
        self.dynamicInsts = list()

    @staticmethod
    def createFromTrace(fileName):
        dg = DataGraph()
        with open(fileName) as trace:
            dynamic_id = 0
            line_buffer = list()
            for line in trace:

                # Notice!!
                # This parse function requires an empty line
                # at the end of the trace to make sure it process
                # the last trace. I am not going to fix this now.

                # debug("Read in: {}".format(line))
                if line[0] != 'i' or len(line_buffer) == 0:
                    line_buffer.append(line)
                    continue
                # Time to break a section.
                static_inst, dynamic_inst = DataGraph.parseInstruction(
                    line_buffer, dynamic_id)

                # Add functions.
                if static_inst.func_name not in dg.functions:
                    dg.functions[static_inst.func_name] = Function(
                        static_inst.func_name)
                func = dg.functions[static_inst.func_name]
                if static_inst.bb_name not in func.bbs:
                    func.bbs[static_inst.bb_name] = BasicBlock(
                        static_inst.bb_name)
                basic_block = func.bbs[static_inst.bb_name]
                # Get or create the static instruction.
                if static_inst.static_id not in basic_block.insts:
                    # This is a new static instruction we have encountered.
                    basic_block.insts[static_inst.static_id] = static_inst
                    # If this instruction generates a result,
                    # add to the function value map.
                    if static_inst.result != '':
                        assert static_inst.result not in func.value_inst_map
                        func.value_inst_map[static_inst.result] = static_inst
                # Get the updated static instruction.
                static_inst = basic_block.insts[static_inst.static_id]
                # Insert the dynamic instruction.
                dg.dynamicInsts.append(dynamic_inst)

                # Update.
                dynamic_id += 1
                line_buffer = list()
                line_buffer.append(line)
        # Add the dependence in another pass.
        DataGraph.addRegDependence(dg)
        return dg

    @staticmethod
    def addRegDependence(dg):
        value_dynamic_inst_map = {}
        for dynamic_inst in dg.dynamicInsts:
            static_inst = dynamic_inst.static_inst
            for operand in static_inst.operands:
                if operand == '':
                    # This is a constant, no name or dependence.
                    continue
                if operand in value_dynamic_inst_map:
                    dynamic_inst.deps.append(value_dynamic_inst_map[operand])
                else:
                    debug("Unknown dependence for {dynamic_id}:{operand}".format(
                        dynamic_id=dynamic_inst.dynamic_id, operand=operand))
            if static_inst.result != '':
                value_dynamic_inst_map[static_inst.result] = dynamic_inst

    # Parse an instruction and return a tupleStaticInst.
    @staticmethod
    def parseInstruction(line_buffer, dynamic_id):
        # debug(line_buffer)
        assert len(line_buffer) > 0
        static_inst = StaticInst()
        dynamic_inst = DynamicInst(dynamic_id, static_inst)
        DataGraph.parseInstLine(line_buffer[0], static_inst)
        for line_id in range(1, len(line_buffer)):
            line = line_buffer[line_id]
            dynamic_value = DynamicValue()
            DataGraph.parseValueLine(line, dynamic_value)
            if line[0] == 'p':
                dynamic_inst.dynamic_values.append(dynamic_value)
                static_inst.operands.append(dynamic_value.name)
            elif line[0] == 'r':
                dynamic_inst.dynamic_result = dynamic_value
                static_inst.result = dynamic_value.name
            else:
                assert False
        return (static_inst, dynamic_inst)

    @staticmethod
    def parseInstLine(line, static_inst):
        fields = line.split('|')
        assert fields[0] == 'i'
        start_pos = 1
        static_inst.func_name = fields[start_pos + 0]
        static_inst.bb_name = fields[start_pos + 1]
        static_inst.static_id = int(fields[start_pos + 2])
        static_inst.op_name = fields[start_pos + 3]

    @staticmethod
    def parseValueLine(line, dynamic_value):
        fields = line.split('|')
        assert fields[0] == 'p' or fields[0] == 'r'
        start_pos = 1
        dynamic_value.name = fields[start_pos + 0]
        dynamic_value.type_name = fields[start_pos + 1]
        dynamic_value.type_id = int(fields[start_pos + 2])
        dynamic_value.value = fields[start_pos + 3]


def print_function(dg, args):
    for _, f in dg.functions.iteritems():
        for _, bb in f.bbs.iteritems():
            for _, static_inst in bb.insts.iteritems():
                debug("{func},{bb},{static_id},{op_name},{result},{operands}".format(
                    func=static_inst.func_name,
                    bb=static_inst.bb_name,
                    static_id=static_inst.static_id,
                    op_name=static_inst.op_name,
                    result=static_inst.result,
                    operands=static_inst.operands
                ))


def print_trace(dg, args):
    for dynamic_inst in dg.dynamicInsts:
        static_inst = dynamic_inst.static_inst
        print("{dynamic_id},{func},{bb},{static_id},{op_name},{result},{operands}".format(
            dynamic_id=dynamic_inst.dynamic_id,
            func=static_inst.func_name,
            bb=static_inst.bb_name,
            static_id=static_inst.static_id,
            op_name=static_inst.op_name,
            result=static_inst.result,
            operands=static_inst.operands
        ))
        dependent_dynamic_ids = list()
        for dependent_dynamic_inst in dynamic_inst.deps:
            dependent_dynamic_ids.append(dependent_dynamic_inst.dynamic_id)
        print("  {dep_ids}".format(dep_ids=dependent_dynamic_ids))


def count_load(dg, args):
    count = 0
    for dynamic_inst in dg.dynamicInsts:
        static_inst = dynamic_inst.static_inst
        if static_inst.op_name == 'load':
            count += 1
    print("Count of load: {loads}".format(loads=count))


def count_store(dg, args):
    count = 0
    for dynamic_inst in dg.dynamicInsts:
        static_inst = dynamic_inst.static_inst
        if static_inst.op_name == 'store':
            count += 1
    print("Count of store: {stores}".format(stores=count))


def print_gem5_llvm_trace_cpu_to_file(dg, args):
    trace_file_name = args[1]
    with open(trace_file_name, 'w') as output:
        for dynamic_inst in dg.dynamicInsts:
            static_inst = dynamic_inst.static_inst
            if static_inst.op_name == 'store':
                assert len(dynamic_inst.dynamic_values) == 2
                output.write("s,1,{vaddr},{size},\n".format(
                    vaddr=dynamic_inst.dynamic_values[1].value,
                    size=4))
            elif static_inst.op_name == 'load':
                assert len(dynamic_inst.dynamic_values) == 1
                output.write("l,1,{vaddr},{size},\n".format(
                    vaddr=dynamic_inst.dynamic_values[0].value,
                    size=4))
            else:
                output.write("c,100,\n")


def main(argv):
    file_name = argv[1]
    dg = DataGraph.createFromTrace(file_name)
    actions = {
        "pr_func": print_function,
        "pr_trace": print_trace,
        "acc_load": count_load,
        "acc_store": count_store,
        "pr_gem5": print_gem5_llvm_trace_cpu_to_file,
    }
    if len(argv) > 2:
        # We have additional arguments, don't use repl.
        if argv[2] not in actions:
            print("Unknown command {cmd}".format(argv[2]))
        else:
            actions[argv[2]](dg, argv[2:])
            return
    while True:
        command = raw_input("> ")
        fields = command.split(' ')
        if fields[0] == 'exit':
            break
        actions[fields[0]](dg, fields)


if __name__ == '__main__':
    main(sys.argv)
