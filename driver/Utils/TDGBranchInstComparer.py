import TDGParser

import sys

"""
This file compares the branch instruction of two tdgs.
Used for debugging the branch predictor.
"""


def parseNextBranchInst(parser):
    while True:
        msg = parser.parseNext()
        if msg is None:
            # We reached the end.
            return msg
        if msg.HasField('branch'):
            return msg


def compareBranchInst(br1, br2):
    if br1.pc != br2.pc:
        return False
    if br1.op != br2.op:
        return False
    if br1.branch.static_next_pc != br2.branch.static_next_pc:
        return False
    if br1.branch.dynamic_next_pc != br2.branch.dynamic_next_pc:
        return False
    if br1.branch.is_conditional != br2.branch.is_conditional:
        return False
    if br1.branch.is_indirect != br2.branch.is_indirect:
        return False
    return True


def main(argv):
    assert(len(argv) == 3)
    fn1 = argv[1]
    fn2 = argv[2]
    parser1 = TDGParser.Gem5TDGParser(fn1)
    parser2 = TDGParser.Gem5TDGParser(fn2)
    while True:
        br1 = parseNextBranchInst(parser1)
        br2 = parseNextBranchInst(parser2)
        if br1 is None and br2 is not None:
            print('Missing branch from TDG 1. Branch from TDG 2:')
            print(br2.__str__())
            break
        if br1 is not None and br2 is None:
            print('Missing branch from TDG 2. Branch from TDG 1:')
            print(br1.__str__())
            break
        if not compareBranchInst(br1, br2):
            print('Mismatch branches. Branch from TDG 1:')
            print(br1.__str__())
            print('Mismatch branches. Branch from TDG 2:')
            print(br2.__str__())
            break
    print('Done.')


if __name__ == '__main__':
    main(sys.argv)
