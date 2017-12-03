add_executable(trace Tracer.cpp)

set_target_properties(trace PROPERTIES
COMPILE_FLAGS "-std=c++14 -fno-rtti"
)