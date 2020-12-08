import subprocess

def symbolicateWithLLDB(binary, pcLatFn):
    # ! LLDB somehow failed to get LineEntry.
    import lldb
    triple = 'x86_64-unknown-linux'
    platform_name = 'gnu'
    add_dependents = False
    debugger = lldb.SBDebugger.Create()
    debugger.SetAsync(False)

    target = debugger.CreateTargetWithFileAndArch(
        binary, triple)
    if not target:
        print('Failed to load binary {b}'.format(b=binary))
        return

    w = open('annotated.' + pcLatFn, 'w')
    with open(pcLatFn) as f:
        for line in f:
            if line.startswith('---'):
                w.write(line)
                continue
            processLine(line, w, target)
    w.close()

def processLine(line, w, target):
    import lldb
    fields = line.split()
    if int(fields[1]) < 100:
        # We ignore these pcs.
        w.write(line)
        return
    pc = int(fields[0], 16)
    so_addr = target.ResolveFileAddress(pc)
    # Get a symbol context for the section offset address which includes
    # a module, compile unit, function, block, line entry, and symbol
    sym_ctx = so_addr.GetSymbolContext(
        lldb.eSymbolContextEverything
        # lldb.eSymbolContextSymbol | lldb.eSymbolContextLineEntry
        )
    print(sym_ctx)
    symbol = sym_ctx.GetSymbol()
    print(sym_ctx.GetCompileUnit())
    if not symbol:
        w.write(line)
        return
    print symbol.GetName()

def symbolicateWithLLVMDwarf(binary, pcLatFn):
    w = open('annotated.' + pcLatFn, 'w')
    with open(pcLatFn) as f:
        for line in f:
            if line.startswith('---'):
                w.write(line)
                continue
            processLineWithLLVMDwarf(line, w, binary)
    w.close()

def processLineWithLLVMDwarf(line, w, binary):
    fields = line.split()
    if int(fields[3]) < 100:
        # We ignore these pcs.
        w.write(line)
        return
    print(fields)
    pc = int(fields[0], 16)
    command = [
        'llvm-dwarfdump',
        binary,
        '--lookup={pc}'.format(pc=pc),
    ]
    output = subprocess.check_output(command)
    for output_line in output.splitlines():
        if output_line.startswith('Line info:'):
            idx = output_line.find('start')
            w.write(line[:-1] + ' ' + output_line[:idx] + '\n')
            return
        call_file = 'DW_AT_call_file'
        call_file_idx = output_line.find(call_file)
        if call_file_idx != -1:
            w.write('    ' + output_line[call_file_idx + len(call_file):])
        call_line = 'DW_AT_call_line'
        call_line_idx = output_line.find(call_line)
        if call_line_idx != -1:
            w.write('    ' + output_line[call_line_idx + len(call_line):] + '\n')
    w.write(line)

if __name__ == '__main__':
    import sys
    binary = sys.argv[1]
    pcLatFn = sys.argv[2]
    symbolicateWithLLVMDwarf(binary, pcLatFn)