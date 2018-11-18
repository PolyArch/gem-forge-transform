
class Gem5Stats:
    def __init__(self, benchmark, fn):
        self.benchmark = benchmark
        self.fn = fn
        self.stats = dict()
        with open(self.fn, 'r') as stats:
            for line in stats:
                if len(line) == 0:
                    continue
                if line[0] == '-':
                    continue
                fields = line.split()
                try:
                    self.stats[fields[0]] = float(fields[1])
                except Exception as e:
                    pass
                    # print('ignore line {line}'.format(line=line))

    def get_default(self, key, default):
        if key in self.stats:
            return self.stats[key]
        else:
            return default

    def get_sim_seconds(self):
        return self['sim_seconds']

    def __getitem__(self, key):
        try:
            return self.stats[key]
        except Exception as e:
            print('Failed to get {key} from {file}'.format(
                key=key,
                file=self.fn
            ))
            raise e

    