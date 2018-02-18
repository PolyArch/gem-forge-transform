import heapq
import sys

import datagraph
import DotGraphExporter


def debug(msg):
    # print(msg)
    pass


def warn(msg):
    print("WARN!: {msg}".format(msg=msg))


def checkConstraints(func, staticInsts):
    # Check if a subgraph matches our constraints on CCA.
    # @param subgraphNodeNames: a set contains the node name in the subgraph.
    # @param staticDeps: the whole static dependence graph.
    # @return boolean: True if it complys with constraints.
    # Check the legality of node.
    for staticInst in staticInsts:
        opName = staticInst.op_name
        if opName == 'load' or opName == 'store':
            return False
        # No control or phi node.
        if opName == 'phi' or opName == 'br':
            return False
    return True


def estimateSpeedUp(func, staticInsts):
    # Estimate the speed up if we map a subgraph to CCA.
    # @return float: the estimated speedup.
    # Hack: ignore empty subgraph.
    if len(staticInsts) <= 1:
        return 1.0
    return 1.2


def getDepStaticInsts(func, graph, staticInst):
    nodeName = DotGraphExporter.getNodeName(staticInst)
    depStaticInsts = set()
    for depNodeName in graph[nodeName]:
        depStaticInst = DotGraphExporter.getStaticInst(func, depNodeName)
        if depStaticInst is None:
            continue
        depStaticInsts.add(depStaticInst)
    return depStaticInsts


def replaceBB(dg, bb, staticInsts, newBB, newStaticInst):
    # Replace the old bb with the new BB.
    # This will also modify the dynamic trace.
    # We modify the dynamic trace by replaying it again
    # so that we also update the dynamic_id.
    newDynamicInsts = list()
    # Store a small environment of the operand for newStaticInst.
    environment = dict()
    lastInsertedReplacedDynamicInst = None
    replaceCount = 0
    for dynamicInst in dg.dynamicInsts:
        staticInst = dynamicInst.static_inst
        if staticInst not in staticInsts:
            # This dynamic inst is not being replaced.
            # We only need to handle its dependence if any of those
            # being replaced.
            newDeps = list()
            anyDepReplaced = False
            for depDynamicInst in dynamicInst.deps:
                # This dep dynamic inst is not being replaced.
                if depDynamicInst.static_inst not in staticInsts:
                    newDeps.append(depDynamicInst)
                else:
                    anyDepReplaced = True
            # If any dep dynamic inst been replaced, we add a dep
            # to the last inserted replaced dynamic inst.
            if anyDepReplaced:
                if lastInsertedReplacedDynamicInst is None:
                    assert False
                newDeps.append(lastInsertedReplacedDynamicInst)
            dynamicInst.deps = newDeps
            # Update the dynamic id and push to newDynamicInsts
            dynamicInst.dynamic_id = len(newDynamicInsts)
            newDynamicInsts.append(dynamicInst)
            for result in staticInst.results:
                if result in newStaticInst.operands:
                    # This is an input we care about.
                    environment[result] = dynamicInst
        elif staticInst.static_id == newStaticInst.static_id:
            # HACK: We only check the static_id match since for now we
            # only deal with replacement in a single basic block.
            # This dynamic inst is being replaced.
            lastInsertedReplacedDynamicInst = datagraph.DynamicInst(
                len(newDynamicInsts), newStaticInst)
            replaceCount += 1
            # HACK: Ignore the dynamic value and result should be no
            # problem?
            # Add all the dependent operand.
            for operand in newStaticInst.operands:
                # TODO: For some input not in the environment,
                # it may be parameter. Add checker for that.
                # Here I just ignore them
                if operand not in environment:
                    warn('Unknown operand {operand}, is it a parameter?'.format(
                        operand=operand))
                    continue
                lastInsertedReplacedDynamicInst.deps.append(
                    environment[operand])
            newDynamicInsts.append(lastInsertedReplacedDynamicInst)
        else:
            # This dynamic inst is simply discarded.
            pass
    # Replace the dynamic insts
    dg.dynamicInsts = newDynamicInsts
    # Fix all the users for static insts.
    func = dg.functions[newStaticInst.func_name]
    for staticInst in staticInsts:
        for operand in staticInst.operands:
            if operand not in func.value_inst_map:
                # Ignore parameter.
                continue
            depStaticInst = func.value_inst_map[operand]
            if depStaticInst in staticInsts:
                # Ignore intermediate results.
                continue
            # Replace the staticInst with newStaticInst.
            if staticInst not in depStaticInst.users[operand]:
                warn(depStaticInst)
                warn(staticInst)
            depStaticInst.users[operand].remove(staticInst)
            depStaticInst.users[operand].add(newStaticInst)
            debug(depStaticInst)
            debug(operand)
            for user in depStaticInst.users[operand]:
                debug('user {x}'.format(x=user))
    # Finaly, replace the bb and fix value map of func.
    func.bbs[bb.name] = newBB
    for result in newStaticInst.results:
        func.value_inst_map[result] = newStaticInst
    return replaceCount


def createReplaceBasicBlock(func, bb, staticInsts):
    # Create a new basic with the subgraph replaced.
    # NOTE: The original func is not modified.
    def findLatestInput():
        # Find the latest input inst id inside this bb.
        # Also find all the input operand.
        latestInputId = -1
        operands = list()
        for staticInst in staticInsts:
            for operand in staticInst.operands:
                if operand not in func.value_inst_map:
                    # Ignore parameter.
                    operands.append(operand)
                    continue
                depStaticInst = func.value_inst_map[operand]
                if depStaticInst in staticInsts:
                    # Ignore intermediate results.
                    continue
                if depStaticInst.bb_name != bb.name:
                    # Ignore inst from other bb.
                    operands.append(operand)
                    continue
                operands.append(operand)
                latestInputId = max(latestInputId, depStaticInst.static_id)
        return (latestInputId, operands)
    # Find the earliest user id inside this bb.

    def findEarliestUser():
        earliestUserId = max(bb.insts.keys()) + 1
        outputs = set()
        outsideUsers = dict()
        for staticInst in staticInsts:
            for result in staticInst.results:
                outputs.add(result)
                outsideUsers[result] = set()
                usedOutside = False
                for user in staticInst.users[result]:
                    if user not in staticInsts:
                        outsideUsers[result].add(user)
                        usedOutside = True
                        if user.bb_name == bb.name:
                            # This is an outside user from same bb.
                            # We care about the static_id to get the
                            # earliest insertion point.
                            earliestUserId = min(
                                earliestUserId, user.static_id)
                if not usedOutside:
                    # Remove the result if not used outside.
                    outputs.remove(result)
                    outsideUsers.pop(result)
        return (earliestUserId, outputs, outsideUsers)
    # Our new static inst can only be inserted at (latestInput, earliestUser)
    latestInputId, operands = findLatestInput()
    earliestUserId, outputs, outsideUsers = findEarliestUser()
    if earliestUserId <= latestInputId + 1:
        raise ValueError('EarliestUserId {earliest} <= LatestInputId {latest} + 1'.format(
            earliest=earliestUserId, latest=latestInputId))

    # Insert at simply the first replaced static inst with in
    # (latestInput, earliestUser)
    insertPoint = -1
    for staticInst in staticInsts:
        if staticInst.static_id > latestInputId and staticInst.static_id < earliestUserId:
            insertPoint = staticInst.static_id
            break
    if insertPoint == -1:
        raise ValueError('Failed to find a insert point')

    # Create the new basic block.
    newBB = datagraph.BasicBlock(bb.name)
    # Copy all the other insts not in the subgraph.
    for staticInst in bb.insts.values():
        if staticInst not in staticInsts:
            newBB.insts[staticInst.static_id] = staticInst
    # Create the new static inst.
    newStaticInst = datagraph.StaticInst()
    newStaticInst.func_name = func.name
    newStaticInst.bb_name = bb.name
    newStaticInst.op_name = 'cca'
    newStaticInst.static_id = insertPoint
    newStaticInst.operands = operands
    newStaticInst.results = outputs
    newStaticInst.users = outsideUsers
    # HACK: For now the context would be a latency, and we set it
    # to fixed 2 cycles.
    newStaticInst.context = 2
    # Add to the new basic block.
    newBB.insts[newStaticInst.static_id] = newStaticInst
    return (newBB, newStaticInst)


def replaceSubgraph(dg, func, bb, staticInsts):
    # Replace the staticInsts with a single CCA inst.
    oldDynamicInstCount = len(dg.dynamicInsts)
    newBB, newStaticInst = createReplaceBasicBlock(func, bb, staticInsts)
    count = replaceBB(dg, bb, staticInsts, newBB, newStaticInst)
    warn('Replaced count {count}, old {old}, new {new}'.format(
        count=count, old=oldDynamicInstCount, new=len(dg.dynamicInsts)))


def CCATransform(dg, funcName, bbName, threshold):
    func = dg.functions[funcName]
    bb = dg.functions[funcName].bbs[bbName]
    graph = DotGraphExporter.extractGraph(func, bb.insts.values())
    matchedStaticInsts = set()
    subgraphs = list()
    for idx in reversed(xrange(len(bb.insts))):
        staticInst = bb.insts[idx]
        if staticInst in matchedStaticInsts:
            continue
        currentSubgraph = set()
        pqueue = list()
        heapq.heappush(pqueue, staticInst)
        while pqueue:
            candidataInst = heapq.heappop(pqueue)
            # Add candidate to the currentSubgraph.
            currentSubgraph.add(candidataInst)
            if not checkConstraints(func, currentSubgraph):
                currentSubgraph.remove(candidataInst)
                continue
            # Add the candidate to the global matched set.
            matchedStaticInsts.add(candidataInst)
            # Add all parent into the pqueue.
            for depStaticInst in getDepStaticInsts(func, graph, candidataInst):
                if depStaticInst not in matchedStaticInsts:
                    heapq.heappush(pqueue, depStaticInst)
        # Check if mapped to CCA will have more gain.
        if estimateSpeedUp(func, currentSubgraph) > threshold:
            subgraphs.append(currentSubgraph)
        else:
            # Clear the current subgraph from the matched node.
            for inst in currentSubgraph:
                matchedStaticInsts.remove(inst)
    # Print all the matched subgraph.
    for idx in xrange(len(subgraphs)):
        subgraph = DotGraphExporter.extractGraph(func, subgraphs[idx])
        subgraphName = 'subgraph_{idx}'.format(idx=idx)
        DotGraphExporter.toDot(subgraphName + '.dot', subgraphName, subgraph)
    # Replace all the subgraph and print out the replaced BB.
    for idx in xrange(len(subgraphs)):
    # for idx in xrange(1):
        warn('Replacing {idx}/{all}'.format(idx=idx, all=len(subgraphs)))
        replaceSubgraph(dg, func, bb, subgraphs[idx])
        # Get the new BB.
        bb = func.bbs[bbName]
        # Print it out.
        subgraph = DotGraphExporter.extractGraph(func, bb.insts.values())
        subgraphName = 'replaced_{idx}'.format(idx=idx)
        DotGraphExporter.toDot(subgraphName + '.dot', subgraphName, subgraph)


def main(argv):
    fn = argv[1]
    dg = datagraph.DataGraph.createFromTrace(fn)
    funcName = 'fft'
    bbName = 'if.then'
    threshold = 1.1
    CCATransform(dg, funcName, bbName, threshold)
    # Print to a gem5 trace file.
    outputFn = argv[2]
    datagraph.print_gem5_llvm_trace_cpu_to_file(dg, ['', outputFn])


if __name__ == '__main__':
    main(sys.argv)
