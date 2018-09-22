
# Protobuf stream messages.
protobuf_generate_cpp(STREAM_PROTOBUF_SRCS STREAM_PROTOBUF_HDRS
    StreamMessage.proto
)

add_library(ProtobufStreamMessage OBJECT  
    ${STREAM_PROTOBUF_SRCS}
)
set_target_properties(ProtobufStreamMessage PROPERTIES
    COMPILE_FLAGS "-std=c++14 -O3 -fPIC -fno-rtti -DGOOGLE_PROTOBUF_NO_RTTI"
)

add_subdirectory(ae)