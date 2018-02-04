import sys


def debug(msg):
    # print(msg)
    pass


def warn(msg):
    print("WARN!: " + msg)


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
        self.insts = dict()


class Function:
    def __init__(self, name):
        self.name = name
        self.bbs = dict()
        # A map from LLVM value to the static inst.
        self.value_inst_map = dict()


class DynamicValue(object):
    def __init__(self):
        self.name = ''
        self.type_name = ''
        self.type_id = -1
        self.value = ''
        # Used for dynamic address value.
        self.base = ''
        self.offset = -1


class DynamicParameter(DynamicValue):
    # Represent a dynamic parameter, with one more field
    # to indicate which parameter it is. e.g the first one, the second one.

    def __init__(self):
        super(DynamicParameter, self).__init__()
        self.index = -1


class DynamicInst:
    def __init__(self, dynamic_id, static_inst):
        self.dynamic_id = dynamic_id
        self.static_inst = static_inst
        self.deps = list()
        self.dynamic_values = list()
        self.dynamic_result = None
        self.environment = dict()


class DataGraph:
    def __init__(self):
        self.functions = dict()
        self.dynamicInsts = list()

    @staticmethod
    def isLegalLine(line):
        if len(line) == 0:
            return False
        return (line[0] == 'i' or line[0] == 'p' or
                line[0] == 'r' or line[0] == 'a' or line[0] == 'e')

    @staticmethod
    def isBreakLine(line):
        return line[0] == 'i' or line[0] == 'e'

    # Return the new dynamic id.
    @staticmethod
    def processLineBuf(dg, line_buffer, environments, stack, dynamic_id):
        # Time to break a section.
        if line_buffer[0][0] == 'i':
            static_inst, dynamic_inst = DataGraph.parseInstruction(
                line_buffer, dynamic_id)

            # Get the function, which is created when enter this
            # function.
            assert static_inst.func_name in dg.functions

            # Update the stack if we are going up the stack.
            while len(stack) > 0 and static_inst.func_name != stack[-1]:
                debug("Leaving {func}".format(func=stack[-1]))
                stack.pop()
                environments.pop()
            # Make sure we are in the correct frame.
            assert len(
                stack) > 0 and static_inst.func_name == stack[-1]

            func = dg.functions[static_inst.func_name]
            # Create the BB if not exist yet.
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
            # Update dynamic environment to the latest one.
            dynamic_inst.environment = environments[-1]
            # Insert the dynamic instruction.
            dg.dynamicInsts.append(dynamic_inst)

            # Update dynamic_id.
            return dynamic_id + 1
        else:
            # Special case for enter function node.
            func_name, environment = DataGraph.parseFuncEnter(
                line_buffer)
            # Create the function.
            if func_name not in dg.functions:
                dg.functions[func_name] = Function(
                    func_name)
            # Update the stack.
            environments.append(environment)
            stack.append(func_name)
            # Dynamic_id is not changed.
            return dynamic_id

    @staticmethod
    def createFromTrace(fileName):
        dg = DataGraph()
        # Record the stack and parameters.
        environments = list()
        stack = list()
        with open(fileName) as trace:
            dynamic_id = 0
            line_buffer = list()
            for line in trace:

                # Notice!!
                # This parse function requires an empty line
                # at the end of the trace to make sure it process
                # the last trace. I am not going to fix this now.

                # Hack: ignore other lines.
                if not DataGraph.isLegalLine(line):
                    continue

                # debug("Read in: {}".format(line))
                if (not DataGraph.isBreakLine(line)) or len(line_buffer) == 0:
                    line_buffer.append(line)
                    continue

                dynamic_id = DataGraph.processLineBuf(
                    dg, line_buffer, environments, stack, dynamic_id)

                line_buffer = list()
                line_buffer.append(line)

            # Process the last line if possible.
            if len(line_buffer) != 0:
                dynamic_id = DataGraph.processLineBuf(
                    dg, line_buffer, environments, stack, dynamic_id)

        # Add the register dependence in another pass.
        DataGraph.addRegDependence(dg)
        # Add the memory dependence, must be done before
        # initializing base/offset.
        DataGraph.addMemDependence(dg)
        # Add the blocking dependence for ret and call inst.
        DataGraph.addBlockDependence(dg)
        # Initialize the base/offset of vaddr.
        DataGraph.initBaseOffsetForDynamicValues(dg)
        return dg

    # Add register dependence.
    @staticmethod
    def addRegDependence(dg):
        value_dynamic_inst_map = {}
        for dynamic_inst in dg.dynamicInsts:
            for dynamic_value in dynamic_inst.dynamic_values:
                operand = dynamic_value.name
                if operand == '':
                    # This is a constant, no name or dependence.
                    continue
                if dynamic_value.type_name == 'label':
                    # This is a label, just ignore it.
                    continue
                if operand in dynamic_inst.environment:
                    # This is just a environment variable, ignore it.
                    continue
                if operand in value_dynamic_inst_map:
                    dynamic_inst.deps.append(value_dynamic_inst_map[operand])
                else:
                    debug("Unknown dependence for {dynamic_id}:{operand}".format(
                        dynamic_id=dynamic_inst.dynamic_id, operand=operand))
            if not dynamic_inst.dynamic_result is None:
                value_dynamic_inst_map[dynamic_inst.dynamic_result.name] = dynamic_inst

    # Add memory dependence.
    # Must be done before initializing base/offset.
    @staticmethod
    def addMemDependence(dg):
        mem_addr_store_dynamic_inst_map = dict()
        mem_addr_load_dynamic_inst_map = dict()
        for dynamic_inst in dg.dynamicInsts:
            static_inst = dynamic_inst.static_inst
            if static_inst.op_name == 'store':
                # For store, the mem addr is the second parameter.
                mem_addr = dynamic_inst.dynamic_values[1].value
                if mem_addr in mem_addr_load_dynamic_inst_map:
                    # write-after-read dependence.
                    for load_inst in mem_addr_load_dynamic_inst_map[mem_addr]:
                        assert load_inst.static_inst.op_name == 'load'
                        assert load_inst.dynamic_values[0].value == mem_addr
                        debug(('Add write-after-read dependence at {mem_addr} '
                               'for {load_dynamic_inst_id} -> {store_dynamic_inst_id}').format(
                            mem_addr=mem_addr,
                            load_dynamic_inst_id=load_inst.dynamic_id,
                            store_dynamic_inst_id=dynamic_inst.dynamic_id))
                        dynamic_inst.deps.append(load_inst)
                if mem_addr in mem_addr_store_dynamic_inst_map:
                    # write-after-write dependence.
                    store_inst = mem_addr_store_dynamic_inst_map[mem_addr]
                    assert store_inst.static_inst.op_name == 'store'
                    assert store_inst.dynamic_values[1].value == mem_addr
                    debug(('Add write-after-write dependence at {mem_addr} '
                           'for {store_dynamic_inst_id0} -> {store_dynamic_inst_id1}').format(
                        mem_addr=mem_addr,
                        store_dynamic_inst_id0=store_inst.dynamic_id,
                        store_dynamic_inst_id1=dynamic_inst.dynamic_id))
                    dynamic_inst.deps.append(store_inst)
                # Add itself into the store map.
                mem_addr_store_dynamic_inst_map[mem_addr] = dynamic_inst
                # Clear the load map as all the future mem inst to this
                # address will dependent on this inst.
                mem_addr_load_dynamic_inst_map[mem_addr] = list()
            elif static_inst.op_name == 'load':
                # For load, the mem addr is the first parameter.
                mem_addr = dynamic_inst.dynamic_values[0].value
                if mem_addr in mem_addr_store_dynamic_inst_map:
                    # read-after-write dependence.
                    store_inst = mem_addr_store_dynamic_inst_map[mem_addr]
                    assert store_inst.static_inst.op_name == 'store'
                    assert store_inst.dynamic_values[1].value == mem_addr
                    debug(('Add read-after-write dependence at {mem_addr} '
                           'for {store_dynamic_inst_id} -> {load_dynamic_inst_id}').format(
                        mem_addr=mem_addr,
                        store_dynamic_inst_id=store_inst.dynamic_id,
                        load_dynamic_inst_id=dynamic_inst.dynamic_id))
                    dynamic_inst.deps.append(store_inst)
                # There is no read-after-read dependence.
                # Add itself into the load map.
                if mem_addr in mem_addr_load_dynamic_inst_map:
                    mem_addr_load_dynamic_inst_map[mem_addr].append(
                        dynamic_inst)
                else:
                    mem_addr_load_dynamic_inst_map[mem_addr] = [dynamic_inst]

    # Add block dependence for some inst, e.g. call, ret.
    # Maintain a set of all the infly inst, and add dependence to all infly
    # insts to achieve block effect.
    # Must come after all mem_dep and reg_dep is added.
    @staticmethod
    def addBlockDependence(dg):
        infly_inst_ids = set()
        for dynamic_inst in dg.dynamicInsts:
            static_inst = dynamic_inst.static_inst
            if (static_inst.op_name == 'call' or static_inst.op_name == 'ret'):
                # Add dependence to all infly insts.
                for infly_inst_id in infly_inst_ids:
                    debug('Add blocking dep for {infly_inst_id} -> {dynamic_id}'.format(
                        infly_inst_id=infly_inst_id, dynamic_id=dynamic_inst.dynamic_id))
                    dynamic_inst.deps.append(dg.dynamicInsts[infly_inst_id])
            # Update the infly insts.
            for dependent_inst in dynamic_inst.deps:
                if dependent_inst.dynamic_id in infly_inst_ids:
                    infly_inst_ids.remove(dependent_inst.dynamic_id)
            infly_inst_ids.add(dynamic_inst.dynamic_id)

    # Initialize the 'base' and 'offset' fields of dynamic value.
    # For now only support GEP instruction.
    @staticmethod
    def initBaseOffsetForDynamicValues(dg):
        for dynamic_inst in dg.dynamicInsts:
            static_inst = dynamic_inst.static_inst
            environment = dynamic_inst.environment
            if static_inst.op_name == 'alloca':
                dynamic_pointer = dynamic_inst.dynamic_result
                # Assert we haven't has this value in the environment.
                if dynamic_pointer.name in environment:
                    print("{We already have the dynamic value {name}".format(
                        name=dynamic_pointer.name))
                assert dynamic_pointer.name not in environment
                # Set the base/offset to itself.
                dynamic_pointer.base = dynamic_pointer.name
                dynamic_pointer.offset = 0
                # Register the dynamic value.
                debug("Register stack address {name} with base {base} offset {offset}".format(
                    name=dynamic_pointer.name, base=dynamic_pointer.base, offset=dynamic_pointer.offset))
                environment[dynamic_pointer.name] = dynamic_pointer
            elif static_inst.op_name == 'getelementptr':
                # Register new mem object.
                dynamic_pointer = dynamic_inst.dynamic_values[0]
                if dynamic_pointer.name not in environment:
                    print("Failed to find dynamic pointer {name}".format(
                        name=dynamic_pointer.name))
                assert dynamic_pointer.name in environment
                dynamic_base = environment[dynamic_pointer.name]
                # This is the dynamic result.
                dynamic_result = dynamic_inst.dynamic_result
                dynamic_result.base = dynamic_base.base
                dynamic_result.offset = (
                    int(dynamic_result.value, 16) - int(dynamic_base.value, 16) + dynamic_base.offset)
                # Register the result.
                environment[dynamic_result.name] = dynamic_result
                debug("Register mem address {} with base {} offset {}".format(
                    dynamic_result.name, dynamic_result.base, dynamic_result.offset))
            elif static_inst.op_name == 'bitcast':
                # Check if the this is casting a pointer.
                dynamic_pointer = dynamic_inst.dynamic_values[0]
                if dynamic_pointer.type_id != 15:
                    # This is not a pointer cast.
                    continue
                # Check if we can have the base and offset of this pointer.
                if dynamic_pointer.name not in environment:
                    warn(
                        "Unkown base/offset for {name}".format(name=dynamic_pointer.name))
                    continue
                # Set the base/offset the same as parameter.
                dynamic_base = environment[dynamic_pointer.name]
                dynamic_result = dynamic_inst.dynamic_result
                dynamic_result.base = dynamic_base.base
                dynamic_result.offset = dynamic_base.offset
                environment[dynamic_result.name] = dynamic_result
                debug("Register mem address {} with base {} offset {}".format(
                    dynamic_result.name, dynamic_result.base, dynamic_result.offset))
            elif static_inst.op_name == 'call':
                # For a call inst, we have to propagate the base/offset.
                # If somehow the callee is not traced, e.g. sin/cos, then we ignore it.
                if dynamic_inst.dynamic_id == len(dg.dynamicInsts) - 1:
                    # This is the last inst.
                    continue
                if (dg.dynamicInsts[dynamic_inst.dynamic_id + 1].static_inst.func_name == static_inst.func_name):
                    # The next inst is stil in the same function, the callee is not traced.
                    continue
                # Get the environment of the next dynamic inst as we are going to update it.
                # Since all the subsequent dynamic inst in the callee will share the same environment
                # object, we only need to update the next one.
                next_inst_environment = dg.dynamicInsts[dynamic_inst.dynamic_id + 1].environment
                # Check the number of formal/actual arguments match.
                assert (len(dynamic_inst.dynamic_values) -
                        1) == len(next_inst_environment)
                for arg_id in xrange(len(dynamic_inst.dynamic_values) - 1):
                    arg_dynamic_value = dynamic_inst.dynamic_values[arg_id]
                    # Ignore the last dynamic value as it is the callee address.
                    if arg_dynamic_value.type_id != 15:
                        # Not pointer.
                        continue
                    if arg_dynamic_value.name not in environment:
                        warn('Unknown base/offset for dynamic_id {dynamic_id}: {name}'.format(
                            dynamic_id=dynamic_inst.dynamic_id, name=arg_dynamic_value.name))
                        continue
                    dynamic_base = environment[arg_dynamic_value.name]
                    # Replace the base/offset in callee's environment to callers.
                    # TODO: A very hacky and dummy to do this. Should improve this later.
                    found = False
                    for dynamic_parameter in next_inst_environment.values():
                        if dynamic_parameter.index == arg_id:
                            debug("Populate base/offset into callee, {parameter} with base {base} offset {offset}".format(
                                parameter=dynamic_parameter.name, base=dynamic_base.base, offset=dynamic_base.offset))
                            found = True
                            dynamic_parameter.base = dynamic_base.base
                            dynamic_parameter.offset = dynamic_base.offset
                            break
                    if not found:
                        warn('Failed to populate into callee {arg_id} {arg}'.format(
                            arg_id=arg_id, arg=arg_dynamic_value.name))
                        assert found

            elif static_inst.op_name == 'load':
                dynamic_pointer = dynamic_inst.dynamic_values[0]
                assert dynamic_pointer.name in environment
                # Get the updated dynamic address.
                dynamic_inst.dynamic_values[0] = environment[dynamic_pointer.name]
            elif static_inst.op_name == 'store':
                dynamic_pointer = dynamic_inst.dynamic_values[1]
                assert dynamic_pointer.name in environment
                dynamic_inst.dynamic_values[1] = environment[dynamic_pointer.name]

    @staticmethod
    def getTypeSize(type_name):
        if type_name == 'double':
            return 8
        elif type_name[0] == 'i':
            # Arbitrary width integer.
            bit_width = int(type_name[1:])
            return bit_width / 8
        elif type_name[0] == '<' or type_name[0] == '[':
            # Vector type or array
            # Hack: only handles <n x t>
            x_pos = type_name.find('x')
            num_element = int(type_name[1:x_pos])
            element_type_name = type_name[x_pos + 2:-1]
            return num_element * DataGraph.getTypeSize(element_type_name)
        else:
            warn('Unkown type size {type}'.format(type=type_name))
            assert False

    # Helper function to get the element size of a pointer.
    # The parameter must be a dynamic value with type_id == 15.
    # For now only support double*.
    @staticmethod
    def getElementSize(dynamic_pointer):
        assert dynamic_pointer.type_id == 15
        assert dynamic_pointer.type_name[-1] == '*'
        element_type_name = dynamic_pointer.type_name[0:-1]
        return DataGraph.getTypeSize(element_type_name)

    # Return a tuple of function name and a dict of parameters.
    @staticmethod
    def parseFuncEnter(line_buffer):
        assert len(line_buffer) > 0
        func_name = line_buffer[0].split('|')[1]
        environment = dict()
        for line_id in range(1, len(line_buffer)):
            line = line_buffer[line_id]
            dynamic_parameter = DynamicParameter()
            # Set the index for this parameter.
            dynamic_parameter.index = line_id - 1
            DataGraph.parseValueLine(line, dynamic_parameter)
            assert line[0] == 'p'
            # If the parameter is a pointer, set its base/offset
            # to itself.
            if dynamic_parameter.type_id == 15:
                dynamic_parameter.base = dynamic_parameter.name
                dynamic_parameter.offset = 0
            environment[dynamic_parameter.name] = dynamic_parameter
            warn("enter {} with param {} {} = {}".format(
                func_name, dynamic_parameter.index, dynamic_parameter.name, dynamic_parameter.value))
        return (func_name, environment)

    # Parse an instruction and return a tuple of StaticInst and DynamicInst.
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
            # Get all the dependence.
            deps = ','.join(
                [str(dep_dynamic_inst.dynamic_id) for dep_dynamic_inst in dynamic_inst.deps])
            if static_inst.op_name == 'store':
                assert len(dynamic_inst.dynamic_values) == 2
                dynamic_pointer = dynamic_inst.dynamic_values[1]
                output.write('store|{deps}|{base}|{offset}|{trace_vaddr}|{size}|{type_id}|{type_name}|{value}|\n'.format(
                    base=dynamic_pointer.base,
                    offset=dynamic_pointer.offset,
                    trace_vaddr=dynamic_pointer.value,
                    size=DataGraph.getElementSize(dynamic_pointer),
                    type_id=dynamic_inst.dynamic_values[0].type_id,
                    type_name=dynamic_inst.dynamic_values[0].type_name,
                    value=dynamic_inst.dynamic_values[0].value,
                    deps=deps))
            elif static_inst.op_name == 'load':
                assert len(dynamic_inst.dynamic_values) == 1
                dynamic_pointer = dynamic_inst.dynamic_values[0]
                output.write('load|{deps}|{base}|{offset}|{trace_vaddr}|{size}|\n'.format(
                    base=dynamic_pointer.base,
                    offset=dynamic_pointer.offset,
                    trace_vaddr=dynamic_pointer.value,
                    size=DataGraph.getElementSize(dynamic_pointer),
                    deps=deps))
            # Special case for the call/ret inst so that the trace cpu can
            # have the basic information about the stack.
            elif static_inst.op_name == 'call':
                # Special case for sin and cos.
                # Replace call to sin and cos with another inst sin/cos.
                name = 'call'
                callee = dynamic_inst.dynamic_values[-1].name
                if callee == 'sin':
                    name = 'sin'
                elif callee == 'cos':
                    name = 'cos'
                # For now all the call to intrinsic functions is a normal
                # computation instruction.
                # We can't ignore this as all the dynamic_id will be wrong.
                if callee.find('llvm.') == 0:
                    name = 'call_intrinsic'
                output.write('{name}|{deps}|\n'.format(
                    name=name, deps=deps))
            elif static_inst.op_name == 'alloca':
                dynamic_pointer = dynamic_inst.dynamic_result
                output.write('alloca|{deps}|{base}|{trace_vaddr}|{size}|\n'.format(
                    base=dynamic_pointer.name,
                    trace_vaddr=dynamic_pointer.value,
                    size=DataGraph.getElementSize(dynamic_pointer),
                    deps=deps))
            elif static_inst.op_name == 'ret':
                output.write('ret|{deps}|\n'.format(deps=deps))
            else:
                output.write('{op_name}|{deps}|\n'.format(op_name=static_inst.op_name, deps=deps))


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
