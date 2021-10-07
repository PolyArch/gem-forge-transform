
# Protobuf stream messages.
protobuf_generate_cpp(STREAM_PROTOBUF_SRCS STREAM_PROTOBUF_HDRS
    StreamMessage.proto
)
protobuf_generate_python(STREAM_PROTOBUF_PY StreamMessage.proto)

add_library(ProtobufStreamMessage OBJECT  
    ${STREAM_PROTOBUF_SRCS}
)
set_target_properties(ProtobufStreamMessage PROPERTIES
    COMPILE_FLAGS "-std=c++14 -O3 -fPIC -fno-rtti -DGOOGLE_PROTOBUF_NO_RTTI"
)

# Copy the generated python protobuf file.
add_custom_target(PyStreamProtobufSrc ALL
    cp ${STREAM_PROTOBUF_PY} ${PROJECT_BINARY_DIR}/${PYTHON_PROTO_MSG}/
    DEPENDS ${STREAM_PROTOBUF_PY})

add_subdirectory(ae)
add_subdirectory(execution)