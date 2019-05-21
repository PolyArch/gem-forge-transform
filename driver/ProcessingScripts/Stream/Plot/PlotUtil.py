
def get_benchmark_name(b):
    fields = b.split('.')
    if fields[0] == 'sdvbs':
        return fields[1]
    if fields[0] == 'cortex':
        return fields[1]
    if fields[0] == 'spec':
        return fields[1] + '.' + fields[2]
    return b

def geomean(a):
    import numpy as np
    x = np.log(a)
    geomean = np.exp(np.mean(x))
    return geomean
    
