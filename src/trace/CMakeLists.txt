
# Protobuf tracer library.
protobuf_generate_cpp(TRACE_PROTOBUF_SRCS TRACE_PROTOBUF_HDRS
    TraceMessage.proto
)

message(STATUS "DONE BUILDING PROTOBUF")
message(STATUS ${TRACE_PROTOBUF_SRCS})

SET_SOURCE_FILES_PROPERTIES(
    ${TRACE_PROTOBUF_SRCS} 
    ${TRACE_PROTOBUF_HDRS} 
    PROPERTIES GENERATED TRUE
)

add_library(TracerProtobuf STATIC 
    Tracer.cpp
    TracerProtobuf.cpp
    ${TRACE_PROTOBUF_SRCS}
)
target_include_directories(TracerProtobuf PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

set_target_properties(TracerProtobuf PROPERTIES
    COMPILE_FLAGS "-std=c++14 -fno-rtti -DGOOGLE_PROTOBUF_NO_RTTI"
)

# GZip tracer library
add_library(TracerGZip STATIC
    Tracer.cpp
    TracerGZip.cpp
)
set_target_properties(TracerGZip PROPERTIES
    COMPILE_FLAGS "-std=c++14 -fno-rtti"
)