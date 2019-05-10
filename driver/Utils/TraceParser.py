import TraceMessage_pb2
import InstructionUIDMapReader
import sys
import gzip
import os


from google.protobuf.internal.decoder import _DecodeVarint32


class TraceParser(object):
    def __init__(self, fn):
        # Try to find the uid.
        folder = os.path.dirname(fn)
        for f in os.listdir(folder):
            uid_fn = os.path.join(folder, f)
            if os.path.isfile(uid_fn) and uid_fn.endswith('.inst.uid'):
                self.uid_map = InstructionUIDMapReader.InstructionUIDMapReader(
                    uid_fn)
                break
        with gzip.open(fn, 'rb') as f:
            self.buf = f.read()
        self.pos = 0
        self.length = len(self.buf)

    def parseNext(self):
        if self.pos >= self.length:
            return None
        size, self.pos = _DecodeVarint32(self.buf, self.pos)
        msg = TraceMessage_pb2.DynamicLLVMTraceEntry()
        msg.ParseFromString(self.buf[self.pos:self.pos + size])

        self.pos += size

        return msg


def toTxt(fn):
    parser = TraceParser(fn)
    txt_fn = fn + '.txt'
    with open(txt_fn, 'w') as txt:
        while True:
            msg = parser.parseNext()
            if msg is None:
                break
            txt.write(msg)
            txt.write('\n')


def analyzeFunctionWeight(fn):
    parser = TraceParser(fn)
    funcs = dict()
    func_bb_weight = dict()
    count = 0
    while True:
        msg = parser.parseNext()
        if msg is None:
            break
        if not msg.HasField('inst'):
            # Ignore function enter.
            continue
        uid = msg.inst.uid
        descriptor = parser.uid_map.getDescriptor(uid)
        if descriptor.func not in funcs:
            funcs[descriptor.func] = 0
            func_bb_weight[descriptor.func] = dict()
        funcs[descriptor.func] += 1
        bb_weight = func_bb_weight[descriptor.func]
        if descriptor.bb not in bb_weight:
            bb_weight[descriptor.bb] = 0
        bb_weight[descriptor.bb] += 1

        count += 1
        if count % 100000 == 0:
            print count
    func_names = list(funcs.keys())
    func_names.sort(key=lambda x: funcs[x], reverse=True)
    total_insts = sum(funcs.values())
    for func_name in func_names:
        print('{func:30} {percent:.2f} {insts}'.format(
            func=func_name,
            percent=funcs[func_name]/float(total_insts),
            insts=funcs[func_name]
        ))
    bb_weight = func_bb_weight['create_candidates']
    bb_names = list(bb_weight.keys())
    bb_names.sort(key=lambda x: bb_weight[x], reverse=True)
    for bb_name in bb_names:
        print('{bb:30} {insts}'.format(
            bb=bb_name,
            insts=bb_weight[bb_name]
        ))



def main(argv):
    fn = argv[1]

    # toTxt(fn)

    analyzeFunctionWeight(fn)

    # parser = TraceParser(fn)

    # while True:
    #     msg = parser.parseNext()
    #     if msg is None:
    #         break

    #     if count % 10000 == 0:
    #         print count

    #     count += 1

    #     if not msg.HasField('inst'):
    #         # Ignore function enter.
    #         continue

    #     inst = msg.inst
    #     if inst.uid != UID:
    #         continue
    #     print inst
    #     result = inst.result

    #     if result.HasField['v_int']:
    #         continue

    #     print 'Something is wrong'
    #     break


if __name__ == '__main__':
    main(sys.argv)
