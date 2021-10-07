
# Protobuf tracer library.
protobuf_generate_cpp(TRACE_PROTOBUF_SRCS TRACE_PROTOBUF_HDRS
    TraceMessage.proto
)

protobuf_generate_cpp(PROFILE_PROTOBUF_SRCS PROFILE_PROTOBUF_HDRS
    ProfileMessage.proto
)
protobuf_generate_python(PROFILE_PROTOBUF_PY ProfileMessage.proto)

protobuf_generate_cpp(UID_MAP_PROTOBUF_SRCS UID_MAP_PROTOBUF_HDRS
    UIDMap.proto
)
protobuf_generate_python(UID_MAP_PROTOBUF_PY UIDMap.proto)

# Add an object lib for parent folder to use.
add_library(ProtobufTraceMessage OBJECT
    ${TRACE_PROTOBUF_SRCS}
    ${PROFILE_PROTOBUF_SRCS}
    ${UID_MAP_PROTOBUF_SRCS}
)
set_target_properties(ProtobufTraceMessage PROPERTIES
    COMPILE_FLAGS "-std=c++14 -O3 -fPIC -fno-rtti -DGOOGLE_PROTOBUF_NO_RTTI"
)

if (NOT DEFINED ENV{LIBUNWIND_INC_PATH})
	message(FATAL_ERROR "Please specify libunwind include path in LIBUNWIND_INC_PATH")
endif()
set(LIBUNWIND_INC_PATH "$ENV{LIBUNWIND_INC_PATH}")

add_library(TracerProtobuf STATIC 
    Tracer.cpp
    ProfileLogger.cpp
    ${UID_MAP_PROTOBUF_SRCS}
    InstructionUIDMapReader.cpp
    ${PROFILE_PROTOBUF_SRCS}
    ../ProtobufSerializer.cpp
    TracerProtobuf.cpp
    ${TRACE_PROTOBUF_SRCS}
    BasicBlockTracer.cpp
)
target_include_directories(TracerProtobuf PUBLIC 
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIBUNWIND_INC_PATH}
)

set_target_properties(TracerProtobuf PROPERTIES
    COMPILE_FLAGS "-std=c++14 -O3 -fno-rtti -DGOOGLE_PROTOBUF_NO_RTTI"
)

# Copy the generated python protobuf file.
add_custom_target(PyProtobufSrc ALL
    cp ${UID_MAP_PROTOBUF_PY} ${PROJECT_BINARY_DIR}/${PYTHON_PROTO_MSG}/
    COMMAND cp ${PROFILE_PROTOBUF_PY} ${PROJECT_BINARY_DIR}/${PYTHON_PROTO_MSG}/
    DEPENDS ${UID_MAP_PROTOBUF_PY} ${PROFILE_PROTOBUF_PY})
