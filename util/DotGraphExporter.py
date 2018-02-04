import sys

import datagraph


def getNodeName(staticInst):
    return "{funcName}_{static_id}_{op_name}".format(
        funcName=staticInst.func_name,
        static_id=staticInst.static_id,
        op_name=staticInst.op_name)

# Print the data graph for a basic block to
# outFn (dot format).
# dg should contains the dependence information.


def bbToDot(outFn, dg, funcName, bbName):
    def isExternal(staticInst):
        return (staticInst.func_name !=
                funcName or staticInst.bb_name != bbName)
    bb = dg.functions[funcName].bbs[bbName]
    graph = dict()
    env = dict()
    for staticInst in bb.insts.values():
        nodeName = getNodeName(staticInst)
        graph[nodeName] = set()
        for operand in staticInst.operands:
            if operand not in env:
                env[operand] = 'External'
            graph[nodeName].add(env[operand])
        # Add the result.
        if staticInst.result != '':
            env[staticInst.result] = nodeName
    with open(outFn, 'w') as out:
        # Replace '.' with '_' to comply with dot file syntax
        out.write('digraph {funcName}_{bbName} {{\n'.format(
            funcName=funcName, bbName=bbName).replace('.', '_'))
        for node in graph:
            for depNode in graph[node]:
                out.write(
                    '{depNode} -> {node};\n'.format(depNode=depNode, node=node))
        out.write('}')


def main(argv):
    fn = argv[1]
    outFn = argv[2]
    dg = datagraph.DataGraph.createFromTrace(fn)
    funcName = 'fft'
    bbName = 'for.body2'
    bbToDot(outFn, dg, funcName, bbName)


if __name__ == '__main__':
    main(sys.argv)
