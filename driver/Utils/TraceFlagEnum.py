
from enum import Enum


class GemForgeTraceROI(Enum):
    """
    Helper enum to communicate ROI with the tracer.
    """
    All = 0
    SpecifiedFunction = 1


class GemForgeTraceMode(Enum):
    """
    Helper enum to communicate trace mode with the tracer.
    """
    Profile = 0
    TraceAll = 1
    TraceUniformInterval = 2
    TraceSpecifiedInterval = 3
