
# Protobuf tracer library.
protobuf_generate_cpp(TRACE_PROTOBUF_SRCS TRACE_PROTOBUF_HDRS
    TraceMessage.proto
)

protobuf_generate_cpp(PROFILE_PROTOBUF_SRCS PROFILE_PROTOBUF_HDRS
    ProfileMessage.proto
)

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

set(LIBUNWIND_INC_PATH "$ENV{LIBUNWIND_INC_PATH}")
if (${LIBUNWIND_INC_PATH} STREQUAL "")
  message(fatal_error "Please specify where to find libunwind include path")
endif()

add_library(TracerProtobuf STATIC 
    Tracer.cpp
    ProfileLogger.cpp
    ${UID_MAP_PROTOBUF_SRCS}
    InstructionUIDMapReader.cpp
    ${PROFILE_PROTOBUF_SRCS}
    ../ProtobufSerializer.cpp
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

# Copy the generated python protobuf file to benchmark/Utils/.
add_custom_target(PyUIDMapProtobufSrc ALL
    cp ${UID_MAP_PROTOBUF_PY} ${PROJECT_SOURCE_DIR}/driver/Utils/
    DEPENDS ${UID_MAP_PROTOBUF_PY})