import sys

import datagraph


def getNodeName(staticInst):
    return 'inst_{func_name}_{bb_name}_{static_id}_{op_name}'.format(
        func_name=staticInst.func_name,
        bb_name=staticInst.bb_name,
        static_id=staticInst.static_id,
        op_name=staticInst.op_name)

def getStaticInst(func, nodeName):
    # Special case for special node.
    if nodeName.find('_') == -1:
        return None
    fields = nodeName.split('_')
    if fields[0] != 'inst':
        return None
    bbName = fields[2]
    staticId = int(fields[3])
    return func.bbs[bbName].insts[staticId]

def toDot(outFn, graphName, graph):
    def dotComplyWrite(f, line):
        f.write(line.replace('.', '_'))
    with open(outFn, 'w') as out:
        # Replace '.' with '_' to comply with dot file syntax
        dotComplyWrite(out, 'digraph {graphName} {{\n'.format(
            graphName=graphName))
        for node in graph:
            for depNode in graph[node]:
                dotComplyWrite(out,
                               '{depNode} -> {node};\n'.format(
                                   depNode=depNode, node=node))
        dotComplyWrite(out, '}')


def extractGraph(func, staticInsts):
    # Extract a subgraph from a function.
    # @func: the function.
    # @staticInsts: a set of static instruction as node in the graph.
    # @return: a map from node name to a set of dependent node names.
    graph = dict()

    def addNode(nodeName):
        if nodeName not in graph:
            graph[nodeName] = set()
    for staticInst in staticInsts:
        nodeName = getNodeName(staticInst)
        addNode(nodeName)
        for operand in staticInst.operands:
            # By default this value is not produced by static inst in the graph.
            # Take it as an input.
            inputNodeName = 'input_{operand}'.format(operand=operand)
            if operand in func.value_inst_map:
                depStaticInst = func.value_inst_map[operand]
                if depStaticInst in staticInsts:
                    inputNodeName = getNodeName(depStaticInst)
            addNode(inputNodeName)
            graph[nodeName].add(inputNodeName)
    # Check for output node.
    for staticInst in staticInsts:
        # Check if the output is used outside the subgraph.
        outputs = set()
        for result in staticInst.results:
            for user in staticInst.users[result]:
                if user not in staticInsts:
                    # This output is used outside the subgraph.
                    outputs.add(result)
                    break
        # Add an output node for this inst.
        nodeName = getNodeName(staticInst)
        # HACK: Generate an output node for all the result generated.
        # This is not the most accurate way.
        for result in outputs:
            outputNodeName = 'output_{result}'.format(result=result)
            addNode(outputNodeName)
            graph[outputNodeName].add(nodeName)
    return graph


def bbToDot(outFn, dg, funcName, bbName):
    # Print the data graph for a basic block to
    # outFn (dot format).
    # dg should contains the dependence information.
    func = dg.functions[funcName]
    bb = dg.functions[funcName].bbs[bbName]
    graph = extractGraph(func, bb.insts.values())
    graphName = '{funcName}_{bbName}'.format(
        funcName=funcName, bbName=bbName).replace('.', '_')
    toDot(outFn, graphName, graph)


def main(argv):
    fn = argv[1]
    outFn = argv[2]
    dg = datagraph.DataGraph.createFromTrace(fn)
    funcName = 'fft'
    bbName = 'if.then'
    bbToDot(outFn, dg, funcName, bbName)


if __name__ == '__main__':
    main(sys.argv)
