
class McPATPower:
    def __init__(self, c, idx):
        self.c = c
        self.idx = idx
        self.dynamic_power = 0.0
        self.gate_leakage_power = 0.0
        self.subthreshold_leakage_power = 0.0
        self.total_power = 0.0

    def get_dyn_power(self):
        return self.dynamic_power

    def get_static_power(self):
        return self.gate_leakage_power + self.subthreshold_leakage_power

    def get_name(self):
        return '{c}.{idx}'.format(c=self.c, idx=self.idx)

    def __str__(self):
        return '{c}.{idx} Runtime Dynamic {dynamic_power} W'.format(c=self.c, idx=self.idx, dynamic_power=self.dynamic_power)


class McPAT:

    COMPONENTS = {
        'processor': 'Processor:',
        'core': 'Core:',
        'ifu': 'Instruction Fetch Unit:',
        'icache': 'Instruction Cache:',
        'ibuffer': 'Instruction Buffer:',
        'decoder': 'Instruction Decoder',
        'rename': 'Renaming Unit',
        'lsu': 'Load Store Unit',
        'mmu': 'Memory Management Unit:',
        'exec': 'Execution Unit:',
        'sche': 'Instruction Scheduler:',
        'rob': 'ROB:',
        # Stupid double spaces to avoid collision with fiq.
        'iq': '  Instruction Window:',
        'fiq': 'FP Instruction Window:',
        'reg': 'Register Files:',
        'alu': 'Integer ALUs',
        'mult': 'Complex ALUs',
        'fpu': 'Floatting Point Units',
        'noc': 'NOC',
        'l2': 'L2',
        'l3': 'L3',
    }

    def __init__(self, fn):
        self.failed = False
        for c in McPAT.COMPONENTS:
            self.__dict__[c] = list()
        try:
            with open(fn) as f:
                current_component = None
                for line in f:
                    for c in McPAT.COMPONENTS:
                        name = McPAT.COMPONENTS[c]
                        if name not in line:
                            continue
                        current_component = McPATPower(c, len(self.__dict__[c]))
                        self.__dict__[c].append(current_component)
                        break
                    if current_component is None:
                        continue
                    fields = line.split()
                    if 'Runtime Dynamic =' in line:
                        current_component.dynamic_power = float(fields[3])
                        # print(current_component)
                        # Runtime Dynamic is always the last field.
                        current_component = None
                    elif 'Gate Leakage =' in line:
                        current_component.gate_leakage_power = float(fields[3])
                    elif 'Subthreshold Leakage =' in line:
                        current_component.subthreshold_leakage_power = float(
                            fields[3])
        except IOError:
            print('Failed to open McPAT file {fn}.'.format(fn=fn))
            self.failed = True

    def get_system_dynamic_power(self):
        if self.failed:
            return float('nan')
        return self.processor[0].get_dyn_power()

    def get_system_static_power(self):
        if self.failed:
            return float('nan')
        system = self.processor[0]
        return system.get_static_power()

    def get_system_power(self):
        return self.get_system_dynamic_power() + self.get_system_static_power()
