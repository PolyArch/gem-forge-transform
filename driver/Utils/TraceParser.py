import TraceMessage_pb2
import sys
import gzip

from google.protobuf.internal.decoder import _DecodeVarint32


class TraceParser(object):
    def __init__(self, fn):
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


def main(argv):
    fn = argv[1]

    toTxt(fn)

    # parser = TraceParser(fn)

    # UID = 30057632
    # count = 0

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
