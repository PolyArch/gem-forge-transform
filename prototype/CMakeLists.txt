add_library(PrototypePass MODULE
    Prototype.cpp
    DynamicTrace.cpp
    Replay.cpp
    StreamReplay.cpp
    LocateAccelerableFunctions.cpp
    GZUtil.cpp
)

# LLVM is (typically) built with no C++ RTTI. We need to match that;
# otherwise, we'll get linker errors about missing RTTI data.
set_target_properties(PrototypePass PROPERTIES
    COMPILE_FLAGS "-std=c++14 -fno-rtti"
)

target_link_libraries(PrototypePass -lz)
