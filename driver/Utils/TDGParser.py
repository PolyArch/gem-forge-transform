import TDGInstruction_pb2
import sys
import gzip

from google.protobuf.internal.decoder import _DecodeVarint32


class Gem5TDGParser(object):
    def __init__(self, fn):
        with gzip.open(fn, 'rb') as f:
            self.buf = f.read()
        # Skip the first 4 byte magic number for gem5.
        self.pos = 4
        self.length = len(self.buf)
        assert(self.pos < self.length)
        size, self.pos = _DecodeVarint32(self.buf, self.pos)
        print("StaticInfo size {size}, {pos}".format(size=size, pos=self.pos))
        self.static_info = TDGInstruction_pb2.StaticInformation()
        self.static_info.ParseFromString(self.buf[self.pos:self.pos + size])
        self.pos += size

    def parseNext(self):
        if self.pos >= self.length:
            return None
        size, self.pos = _DecodeVarint32(self.buf, self.pos)
        print("TDGInstruction size {size}, {pos}".format(
            size=size, pos=self.pos))
        msg = TDGInstruction_pb2.TDGInstruction()
        msg.ParseFromString(self.buf[self.pos:self.pos + size])

        self.pos += size
        print(msg)

        return msg


def toTxt(fn):
    parser = Gem5TDGParser(fn)
    txt_fn = fn + '.txt'
    with open(txt_fn, 'w') as txt:
        while True:
            msg = parser.parseNext()
            if msg is None:
                break
            txt.write(msg.__str__())
            txt.write('\n')


def main(argv):
    fn = argv[1]

    toTxt(fn)


if __name__ == '__main__':
    main(sys.argv)
