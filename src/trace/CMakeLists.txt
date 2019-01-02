
# Protobuf tracer library.
protobuf_generate_cpp(TRACE_PROTOBUF_SRCS TRACE_PROTOBUF_HDRS
    TraceMessage.proto
)

protobuf_generate_cpp(PROFILE_PROTOBUF_SRCS PROFILE_PROTOBUF_HDRS
    ProfileMessage.proto
)

message(STATUS "DONE BUILDING PROTOBUF")
message(STATUS ${TRACE_PROTOBUF_SRCS})
message(STATUS ${PROFILE_PROTOBUF_SRCS})

add_library(ProtobufTraceMessage OBJECT
    ${TRACE_PROTOBUF_SRCS}
    ${PROFILE_PROTOBUF_SRCS}
)
set_target_properties(ProtobufTraceMessage PROPERTIES
    COMPILE_FLAGS "-std=c++14 -O3 -fPIC -fno-rtti -DGOOGLE_PROTOBUF_NO_RTTI"
)

set(LIBUNWIND_INC_PATH "$ENV{LIBUNWIND_INC_PATH}")
if (${LIBUNWIND_INC_PATH} STREQUAL "")
  message(fatal_error "Please specify where to find libunwind include path")
endif()

add_library(TracerProtobuf STATIC 
    Tracer.cpp
    ProfileLogger.cpp
    ${PROFILE_PROTOBUF_SRCS}
    TracerProtobuf.cpp
    ${TRACE_PROTOBUF_SRCS}
)
target_include_directories(TracerProtobuf PUBLIC 
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIBUNWIND_INC_PATH}
)

set_target_properties(TracerProtobuf PROPERTIES
    COMPILE_FLAGS "-std=c++14 -O3 -fno-rtti -DGOOGLE_PROTOBUF_NO_RTTI"
)

# GZip tracer library
add_library(TracerGZip STATIC
    Tracer.cpp
    ProfileLogger.cpp
    ${PROFILE_PROTOBUF_SRCS}
    TracerGZip.cpp
)

target_include_directories(TracerGZip PUBLIC 
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIBUNWIND_INC_PATH}
)

set_target_properties(TracerGZip PROPERTIES
    COMPILE_FLAGS "-std=c++14 -fno-rtti -DGOOGLE_PROTOBUF_NO_RTTI"
)