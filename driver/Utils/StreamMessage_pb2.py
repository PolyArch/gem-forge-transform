# Generated by the protocol buffer compiler.  DO NOT EDIT!
# source: StreamMessage.proto

import sys
_b=sys.version_info[0]<3 and (lambda x:x) or (lambda x:x.encode('latin1'))
from google.protobuf.internal import enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from google.protobuf import reflection as _reflection
from google.protobuf import symbol_database as _symbol_database
from google.protobuf import descriptor_pb2
# @@protoc_insertion_point(imports)

_sym_db = _symbol_database.Default()




DESCRIPTOR = _descriptor.FileDescriptor(
  name='StreamMessage.proto',
  package='LLVM.TDG',
  syntax='proto3',
  serialized_pb=_b('\n\x13StreamMessage.proto\x12\x08LLVM.TDG\"\xe0\x04\n\x10StaticStreamInfo\x12\x14\n\x0cis_candidate\x18\x01 \x01(\x08\x12\x14\n\x0cis_qualified\x18\x02 \x01(\x08\x12\x11\n\tis_chosen\x18\x03 \x01(\x08\x12\x30\n\x0bstp_pattern\x18\x04 \x01(\x0e\x32\x1b.LLVM.TDG.StreamStepPattern\x12\x31\n\x0bval_pattern\x18\x05 \x01(\x0e\x32\x1c.LLVM.TDG.StreamValuePattern\x12K\n\x11not_stream_reason\x18\x06 \x01(\x0e\x32\x30.LLVM.TDG.StaticStreamInfo.StaticNotStreamReason\x12\x1a\n\x12loop_possible_path\x18\x07 \x01(\r\x12!\n\x19\x63onfig_loop_possible_path\x18\x08 \x01(\r\"\x9b\x02\n\x15StaticNotStreamReason\x12\x0b\n\x07UNKNOWN\x10\x00\x12\x30\n,BASE_STREAM_INNER_MOST_LOOP_NOT_CONTAIN_MINE\x10\x01\x12#\n\x1fMULTI_PHIMNODE_FOR_COMPUTEMNODE\x10\x02\x12 \n\x1cMULTI_NON_EMPTY_COMPUTE_PATH\x10\x03\x12\x1d\n\x19NOT_SCEVABLE_COMPUTEMNODE\x10\x04\x12\x12\n\x0eRANDOM_PATTERN\x10\x05\x12\x13\n\x0fMULTI_STEP_ROOT\x10\x06\x12\x1d\n\x19\x42\x41SE_STREAM_NOT_QUALIFIED\x10\x08\x12\x15\n\x11NO_STATIC_MAPPING\x10\t\"\xad\x01\n\x11\x44ynamicStreamInfo\x12\x14\n\x0cis_candidate\x18\x01 \x01(\x08\x12\x14\n\x0cis_qualified\x18\x02 \x01(\x08\x12\x11\n\tis_chosen\x18\x03 \x01(\x08\x12\x12\n\nis_aliased\x18\x04 \x01(\x08\x12\x13\n\x0btotal_iters\x18\x05 \x01(\x04\x12\x16\n\x0etotal_accesses\x18\x06 \x01(\x04\x12\x18\n\x10total_configures\x18\x07 \x01(\x04\"\xe5\x04\n\nStreamInfo\x12\x0c\n\x04name\x18\x01 \x01(\t\x12\n\n\x02id\x18\x02 \x01(\x04\x12\x0c\n\x04type\x18\x03 \x01(\t\x12\x12\n\nloop_level\x18\x04 \x01(\r\x12\x19\n\x11\x63onfig_loop_level\x18\x05 \x01(\r\x12\x14\n\x0c\x65lement_size\x18\x06 \x01(\x05\x12\x14\n\x0cpattern_path\x18\x07 \x01(\t\x12\x14\n\x0chistory_path\x18\x08 \x01(\t\x12\x16\n\x0e\x63oalesce_group\x18\t \x01(\x05\x12\x31\n\x0c\x64ynamic_info\x18\n \x01(\x0b\x32\x1b.LLVM.TDG.DynamicStreamInfo\x12\x18\n\x10region_stream_id\x18\x0b \x01(\x05\x12\x16\n\x0e\x61\x64\x64r_func_name\x18\x0c \x01(\t\x12\x33\n\x0c\x62\x61se_streams\x18\x10 \x03(\x0b\x32\x1d.LLVM.TDG.StreamInfo.StreamId\x12\x38\n\x11\x62\x61\x63k_base_streams\x18\x11 \x03(\x0b\x32\x1d.LLVM.TDG.StreamInfo.StreamId\x12:\n\x13\x63hosen_base_streams\x18\x12 \x03(\x0b\x32\x1d.LLVM.TDG.StreamInfo.StreamId\x12?\n\x18\x63hosen_back_base_streams\x18\x13 \x03(\x0b\x32\x1d.LLVM.TDG.StreamInfo.StreamId\x12/\n\x0bstatic_info\x18\x18 \x01(\x0b\x32\x1a.LLVM.TDG.StaticStreamInfo\x1a$\n\x08StreamId\x12\x0c\n\x04name\x18\x01 \x01(\t\x12\n\n\x02id\x18\x02 \x01(\x04\"\xa7\x01\n\x0cStreamRegion\x12\x0e\n\x06region\x18\x01 \x01(\t\x12\x1b\n\x13total_alive_streams\x18\x02 \x01(\x05\x12%\n\x1dtotal_alive_coalesced_streams\x18\x03 \x01(\x05\x12%\n\x07streams\x18\x08 \x03(\x0b\x32\x14.LLVM.TDG.StreamInfo\x12\x1c\n\x14\x63oalesced_stream_ids\x18\t \x03(\x04\"\x8e\x02\n\rStreamPattern\x12\x13\n\x0bval_pattern\x18\x01 \x01(\t\x12\x13\n\x0b\x61\x63\x63_pattern\x18\x02 \x01(\t\x12\r\n\x05iters\x18\x03 \x01(\x04\x12\x10\n\x08\x61\x63\x63\x65sses\x18\x04 \x01(\x04\x12\x0f\n\x07updates\x18\x05 \x01(\x04\x12\x0c\n\x04\x62\x61se\x18\x06 \x01(\x04\x12\x10\n\x08stride_i\x18\x07 \x01(\x03\x12\n\n\x02ni\x18\x08 \x01(\x04\x12\x10\n\x08stride_j\x18\t \x01(\x03\x12\x35\n\x07history\x18\n \x03(\x0b\x32$.LLVM.TDG.StreamPattern.HistoryEntry\x1a,\n\x0cHistoryEntry\x12\r\n\x05valid\x18\x01 \x01(\x08\x12\r\n\x05value\x18\x02 \x01(\x04\"\xbc\x01\n\rStreamHistory\x12\n\n\x02id\x18\x01 \x01(\x04\x12\x35\n\x07history\x18\x02 \x03(\x0b\x32$.LLVM.TDG.StreamHistory.HistoryEntry\x12\x17\n\x0fnum_cache_lines\x18\x03 \x01(\x04\x12\x14\n\x0cnum_accesses\x18\x04 \x01(\x04\x1a\x39\n\x0cHistoryEntry\x12\r\n\x05valid\x18\x01 \x01(\x08\x12\x0c\n\x04\x61\x64\x64r\x18\x02 \x01(\x04\x12\x0c\n\x04used\x18\x03 \x01(\x08*O\n\x11StreamStepPattern\x12\x0b\n\x07UNKNOWN\x10\x00\x12\x11\n\rUNCONDITIONAL\x10\x01\x12\x0f\n\x0b\x43ONDITIONAL\x10\x02\x12\t\n\x05NEVER\x10\x03*x\n\x12StreamValuePattern\x12\n\n\x06RANDOM\x10\x00\x12\x0c\n\x08\x43ONSTANT\x10\x01\x12\n\n\x06LINEAR\x10\x02\x12\x0c\n\x08QUARDRIC\x10\x03\x12\x0c\n\x08INDIRECT\x10\x04\x12\x11\n\rPOINTER_CHASE\x10\x05\x12\r\n\tPREV_LOAD\x10\x06\x62\x06proto3')
)

_STREAMSTEPPATTERN = _descriptor.EnumDescriptor(
  name='StreamStepPattern',
  full_name='LLVM.TDG.StreamStepPattern',
  filename=None,
  file=DESCRIPTOR,
  values=[
    _descriptor.EnumValueDescriptor(
      name='UNKNOWN', index=0, number=0,
      options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='UNCONDITIONAL', index=1, number=1,
      options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='CONDITIONAL', index=2, number=2,
      options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='NEVER', index=3, number=3,
      options=None,
      type=None),
  ],
  containing_type=None,
  options=None,
  serialized_start=2070,
  serialized_end=2149,
)
_sym_db.RegisterEnumDescriptor(_STREAMSTEPPATTERN)

StreamStepPattern = enum_type_wrapper.EnumTypeWrapper(_STREAMSTEPPATTERN)
_STREAMVALUEPATTERN = _descriptor.EnumDescriptor(
  name='StreamValuePattern',
  full_name='LLVM.TDG.StreamValuePattern',
  filename=None,
  file=DESCRIPTOR,
  values=[
    _descriptor.EnumValueDescriptor(
      name='RANDOM', index=0, number=0,
      options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='CONSTANT', index=1, number=1,
      options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='LINEAR', index=2, number=2,
      options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='QUARDRIC', index=3, number=3,
      options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='INDIRECT', index=4, number=4,
      options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='POINTER_CHASE', index=5, number=5,
      options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='PREV_LOAD', index=6, number=6,
      options=None,
      type=None),
  ],
  containing_type=None,
  options=None,
  serialized_start=2151,
  serialized_end=2271,
)
_sym_db.RegisterEnumDescriptor(_STREAMVALUEPATTERN)

StreamValuePattern = enum_type_wrapper.EnumTypeWrapper(_STREAMVALUEPATTERN)
UNKNOWN = 0
UNCONDITIONAL = 1
CONDITIONAL = 2
NEVER = 3
RANDOM = 0
CONSTANT = 1
LINEAR = 2
QUARDRIC = 3
INDIRECT = 4
POINTER_CHASE = 5
PREV_LOAD = 6


_STATICSTREAMINFO_STATICNOTSTREAMREASON = _descriptor.EnumDescriptor(
  name='StaticNotStreamReason',
  full_name='LLVM.TDG.StaticStreamInfo.StaticNotStreamReason',
  filename=None,
  file=DESCRIPTOR,
  values=[
    _descriptor.EnumValueDescriptor(
      name='UNKNOWN', index=0, number=0,
      options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='BASE_STREAM_INNER_MOST_LOOP_NOT_CONTAIN_MINE', index=1, number=1,
      options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='MULTI_PHIMNODE_FOR_COMPUTEMNODE', index=2, number=2,
      options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='MULTI_NON_EMPTY_COMPUTE_PATH', index=3, number=3,
      options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='NOT_SCEVABLE_COMPUTEMNODE', index=4, number=4,
      options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='RANDOM_PATTERN', index=5, number=5,
      options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='MULTI_STEP_ROOT', index=6, number=6,
      options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='BASE_STREAM_NOT_QUALIFIED', index=7, number=8,
      options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='NO_STATIC_MAPPING', index=8, number=9,
      options=None,
      type=None),
  ],
  containing_type=None,
  options=None,
  serialized_start=359,
  serialized_end=642,
)
_sym_db.RegisterEnumDescriptor(_STATICSTREAMINFO_STATICNOTSTREAMREASON)


_STATICSTREAMINFO = _descriptor.Descriptor(
  name='StaticStreamInfo',
  full_name='LLVM.TDG.StaticStreamInfo',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='is_candidate', full_name='LLVM.TDG.StaticStreamInfo.is_candidate', index=0,
      number=1, type=8, cpp_type=7, label=1,
      has_default_value=False, default_value=False,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='is_qualified', full_name='LLVM.TDG.StaticStreamInfo.is_qualified', index=1,
      number=2, type=8, cpp_type=7, label=1,
      has_default_value=False, default_value=False,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='is_chosen', full_name='LLVM.TDG.StaticStreamInfo.is_chosen', index=2,
      number=3, type=8, cpp_type=7, label=1,
      has_default_value=False, default_value=False,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='stp_pattern', full_name='LLVM.TDG.StaticStreamInfo.stp_pattern', index=3,
      number=4, type=14, cpp_type=8, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='val_pattern', full_name='LLVM.TDG.StaticStreamInfo.val_pattern', index=4,
      number=5, type=14, cpp_type=8, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='not_stream_reason', full_name='LLVM.TDG.StaticStreamInfo.not_stream_reason', index=5,
      number=6, type=14, cpp_type=8, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='loop_possible_path', full_name='LLVM.TDG.StaticStreamInfo.loop_possible_path', index=6,
      number=7, type=13, cpp_type=3, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='config_loop_possible_path', full_name='LLVM.TDG.StaticStreamInfo.config_loop_possible_path', index=7,
      number=8, type=13, cpp_type=3, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
    _STATICSTREAMINFO_STATICNOTSTREAMREASON,
  ],
  options=None,
  is_extendable=False,
  syntax='proto3',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=34,
  serialized_end=642,
)


_DYNAMICSTREAMINFO = _descriptor.Descriptor(
  name='DynamicStreamInfo',
  full_name='LLVM.TDG.DynamicStreamInfo',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='is_candidate', full_name='LLVM.TDG.DynamicStreamInfo.is_candidate', index=0,
      number=1, type=8, cpp_type=7, label=1,
      has_default_value=False, default_value=False,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='is_qualified', full_name='LLVM.TDG.DynamicStreamInfo.is_qualified', index=1,
      number=2, type=8, cpp_type=7, label=1,
      has_default_value=False, default_value=False,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='is_chosen', full_name='LLVM.TDG.DynamicStreamInfo.is_chosen', index=2,
      number=3, type=8, cpp_type=7, label=1,
      has_default_value=False, default_value=False,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='is_aliased', full_name='LLVM.TDG.DynamicStreamInfo.is_aliased', index=3,
      number=4, type=8, cpp_type=7, label=1,
      has_default_value=False, default_value=False,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='total_iters', full_name='LLVM.TDG.DynamicStreamInfo.total_iters', index=4,
      number=5, type=4, cpp_type=4, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='total_accesses', full_name='LLVM.TDG.DynamicStreamInfo.total_accesses', index=5,
      number=6, type=4, cpp_type=4, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='total_configures', full_name='LLVM.TDG.DynamicStreamInfo.total_configures', index=6,
      number=7, type=4, cpp_type=4, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
  ],
  options=None,
  is_extendable=False,
  syntax='proto3',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=645,
  serialized_end=818,
)


_STREAMINFO_STREAMID = _descriptor.Descriptor(
  name='StreamId',
  full_name='LLVM.TDG.StreamInfo.StreamId',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='name', full_name='LLVM.TDG.StreamInfo.StreamId.name', index=0,
      number=1, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=_b("").decode('utf-8'),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='id', full_name='LLVM.TDG.StreamInfo.StreamId.id', index=1,
      number=2, type=4, cpp_type=4, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
  ],
  options=None,
  is_extendable=False,
  syntax='proto3',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=1398,
  serialized_end=1434,
)

_STREAMINFO = _descriptor.Descriptor(
  name='StreamInfo',
  full_name='LLVM.TDG.StreamInfo',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='name', full_name='LLVM.TDG.StreamInfo.name', index=0,
      number=1, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=_b("").decode('utf-8'),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='id', full_name='LLVM.TDG.StreamInfo.id', index=1,
      number=2, type=4, cpp_type=4, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='type', full_name='LLVM.TDG.StreamInfo.type', index=2,
      number=3, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=_b("").decode('utf-8'),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='loop_level', full_name='LLVM.TDG.StreamInfo.loop_level', index=3,
      number=4, type=13, cpp_type=3, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='config_loop_level', full_name='LLVM.TDG.StreamInfo.config_loop_level', index=4,
      number=5, type=13, cpp_type=3, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='element_size', full_name='LLVM.TDG.StreamInfo.element_size', index=5,
      number=6, type=5, cpp_type=1, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='pattern_path', full_name='LLVM.TDG.StreamInfo.pattern_path', index=6,
      number=7, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=_b("").decode('utf-8'),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='history_path', full_name='LLVM.TDG.StreamInfo.history_path', index=7,
      number=8, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=_b("").decode('utf-8'),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='coalesce_group', full_name='LLVM.TDG.StreamInfo.coalesce_group', index=8,
      number=9, type=5, cpp_type=1, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='dynamic_info', full_name='LLVM.TDG.StreamInfo.dynamic_info', index=9,
      number=10, type=11, cpp_type=10, label=1,
      has_default_value=False, default_value=None,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='region_stream_id', full_name='LLVM.TDG.StreamInfo.region_stream_id', index=10,
      number=11, type=5, cpp_type=1, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='addr_func_name', full_name='LLVM.TDG.StreamInfo.addr_func_name', index=11,
      number=12, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=_b("").decode('utf-8'),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='base_streams', full_name='LLVM.TDG.StreamInfo.base_streams', index=12,
      number=16, type=11, cpp_type=10, label=3,
      has_default_value=False, default_value=[],
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='back_base_streams', full_name='LLVM.TDG.StreamInfo.back_base_streams', index=13,
      number=17, type=11, cpp_type=10, label=3,
      has_default_value=False, default_value=[],
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='chosen_base_streams', full_name='LLVM.TDG.StreamInfo.chosen_base_streams', index=14,
      number=18, type=11, cpp_type=10, label=3,
      has_default_value=False, default_value=[],
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='chosen_back_base_streams', full_name='LLVM.TDG.StreamInfo.chosen_back_base_streams', index=15,
      number=19, type=11, cpp_type=10, label=3,
      has_default_value=False, default_value=[],
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='static_info', full_name='LLVM.TDG.StreamInfo.static_info', index=16,
      number=24, type=11, cpp_type=10, label=1,
      has_default_value=False, default_value=None,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
  ],
  extensions=[
  ],
  nested_types=[_STREAMINFO_STREAMID, ],
  enum_types=[
  ],
  options=None,
  is_extendable=False,
  syntax='proto3',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=821,
  serialized_end=1434,
)


_STREAMREGION = _descriptor.Descriptor(
  name='StreamRegion',
  full_name='LLVM.TDG.StreamRegion',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='region', full_name='LLVM.TDG.StreamRegion.region', index=0,
      number=1, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=_b("").decode('utf-8'),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='total_alive_streams', full_name='LLVM.TDG.StreamRegion.total_alive_streams', index=1,
      number=2, type=5, cpp_type=1, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='total_alive_coalesced_streams', full_name='LLVM.TDG.StreamRegion.total_alive_coalesced_streams', index=2,
      number=3, type=5, cpp_type=1, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='streams', full_name='LLVM.TDG.StreamRegion.streams', index=3,
      number=8, type=11, cpp_type=10, label=3,
      has_default_value=False, default_value=[],
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='coalesced_stream_ids', full_name='LLVM.TDG.StreamRegion.coalesced_stream_ids', index=4,
      number=9, type=4, cpp_type=4, label=3,
      has_default_value=False, default_value=[],
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
  ],
  options=None,
  is_extendable=False,
  syntax='proto3',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=1437,
  serialized_end=1604,
)


_STREAMPATTERN_HISTORYENTRY = _descriptor.Descriptor(
  name='HistoryEntry',
  full_name='LLVM.TDG.StreamPattern.HistoryEntry',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='valid', full_name='LLVM.TDG.StreamPattern.HistoryEntry.valid', index=0,
      number=1, type=8, cpp_type=7, label=1,
      has_default_value=False, default_value=False,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='value', full_name='LLVM.TDG.StreamPattern.HistoryEntry.value', index=1,
      number=2, type=4, cpp_type=4, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
  ],
  options=None,
  is_extendable=False,
  syntax='proto3',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=1833,
  serialized_end=1877,
)

_STREAMPATTERN = _descriptor.Descriptor(
  name='StreamPattern',
  full_name='LLVM.TDG.StreamPattern',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='val_pattern', full_name='LLVM.TDG.StreamPattern.val_pattern', index=0,
      number=1, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=_b("").decode('utf-8'),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='acc_pattern', full_name='LLVM.TDG.StreamPattern.acc_pattern', index=1,
      number=2, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=_b("").decode('utf-8'),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='iters', full_name='LLVM.TDG.StreamPattern.iters', index=2,
      number=3, type=4, cpp_type=4, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='accesses', full_name='LLVM.TDG.StreamPattern.accesses', index=3,
      number=4, type=4, cpp_type=4, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='updates', full_name='LLVM.TDG.StreamPattern.updates', index=4,
      number=5, type=4, cpp_type=4, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='base', full_name='LLVM.TDG.StreamPattern.base', index=5,
      number=6, type=4, cpp_type=4, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='stride_i', full_name='LLVM.TDG.StreamPattern.stride_i', index=6,
      number=7, type=3, cpp_type=2, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='ni', full_name='LLVM.TDG.StreamPattern.ni', index=7,
      number=8, type=4, cpp_type=4, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='stride_j', full_name='LLVM.TDG.StreamPattern.stride_j', index=8,
      number=9, type=3, cpp_type=2, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='history', full_name='LLVM.TDG.StreamPattern.history', index=9,
      number=10, type=11, cpp_type=10, label=3,
      has_default_value=False, default_value=[],
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
  ],
  extensions=[
  ],
  nested_types=[_STREAMPATTERN_HISTORYENTRY, ],
  enum_types=[
  ],
  options=None,
  is_extendable=False,
  syntax='proto3',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=1607,
  serialized_end=1877,
)


_STREAMHISTORY_HISTORYENTRY = _descriptor.Descriptor(
  name='HistoryEntry',
  full_name='LLVM.TDG.StreamHistory.HistoryEntry',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='valid', full_name='LLVM.TDG.StreamHistory.HistoryEntry.valid', index=0,
      number=1, type=8, cpp_type=7, label=1,
      has_default_value=False, default_value=False,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='addr', full_name='LLVM.TDG.StreamHistory.HistoryEntry.addr', index=1,
      number=2, type=4, cpp_type=4, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='used', full_name='LLVM.TDG.StreamHistory.HistoryEntry.used', index=2,
      number=3, type=8, cpp_type=7, label=1,
      has_default_value=False, default_value=False,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
  ],
  options=None,
  is_extendable=False,
  syntax='proto3',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=2011,
  serialized_end=2068,
)

_STREAMHISTORY = _descriptor.Descriptor(
  name='StreamHistory',
  full_name='LLVM.TDG.StreamHistory',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='id', full_name='LLVM.TDG.StreamHistory.id', index=0,
      number=1, type=4, cpp_type=4, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='history', full_name='LLVM.TDG.StreamHistory.history', index=1,
      number=2, type=11, cpp_type=10, label=3,
      has_default_value=False, default_value=[],
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='num_cache_lines', full_name='LLVM.TDG.StreamHistory.num_cache_lines', index=2,
      number=3, type=4, cpp_type=4, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='num_accesses', full_name='LLVM.TDG.StreamHistory.num_accesses', index=3,
      number=4, type=4, cpp_type=4, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
  ],
  extensions=[
  ],
  nested_types=[_STREAMHISTORY_HISTORYENTRY, ],
  enum_types=[
  ],
  options=None,
  is_extendable=False,
  syntax='proto3',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=1880,
  serialized_end=2068,
)

_STATICSTREAMINFO.fields_by_name['stp_pattern'].enum_type = _STREAMSTEPPATTERN
_STATICSTREAMINFO.fields_by_name['val_pattern'].enum_type = _STREAMVALUEPATTERN
_STATICSTREAMINFO.fields_by_name['not_stream_reason'].enum_type = _STATICSTREAMINFO_STATICNOTSTREAMREASON
_STATICSTREAMINFO_STATICNOTSTREAMREASON.containing_type = _STATICSTREAMINFO
_STREAMINFO_STREAMID.containing_type = _STREAMINFO
_STREAMINFO.fields_by_name['dynamic_info'].message_type = _DYNAMICSTREAMINFO
_STREAMINFO.fields_by_name['base_streams'].message_type = _STREAMINFO_STREAMID
_STREAMINFO.fields_by_name['back_base_streams'].message_type = _STREAMINFO_STREAMID
_STREAMINFO.fields_by_name['chosen_base_streams'].message_type = _STREAMINFO_STREAMID
_STREAMINFO.fields_by_name['chosen_back_base_streams'].message_type = _STREAMINFO_STREAMID
_STREAMINFO.fields_by_name['static_info'].message_type = _STATICSTREAMINFO
_STREAMREGION.fields_by_name['streams'].message_type = _STREAMINFO
_STREAMPATTERN_HISTORYENTRY.containing_type = _STREAMPATTERN
_STREAMPATTERN.fields_by_name['history'].message_type = _STREAMPATTERN_HISTORYENTRY
_STREAMHISTORY_HISTORYENTRY.containing_type = _STREAMHISTORY
_STREAMHISTORY.fields_by_name['history'].message_type = _STREAMHISTORY_HISTORYENTRY
DESCRIPTOR.message_types_by_name['StaticStreamInfo'] = _STATICSTREAMINFO
DESCRIPTOR.message_types_by_name['DynamicStreamInfo'] = _DYNAMICSTREAMINFO
DESCRIPTOR.message_types_by_name['StreamInfo'] = _STREAMINFO
DESCRIPTOR.message_types_by_name['StreamRegion'] = _STREAMREGION
DESCRIPTOR.message_types_by_name['StreamPattern'] = _STREAMPATTERN
DESCRIPTOR.message_types_by_name['StreamHistory'] = _STREAMHISTORY
DESCRIPTOR.enum_types_by_name['StreamStepPattern'] = _STREAMSTEPPATTERN
DESCRIPTOR.enum_types_by_name['StreamValuePattern'] = _STREAMVALUEPATTERN
_sym_db.RegisterFileDescriptor(DESCRIPTOR)

StaticStreamInfo = _reflection.GeneratedProtocolMessageType('StaticStreamInfo', (_message.Message,), dict(
  DESCRIPTOR = _STATICSTREAMINFO,
  __module__ = 'StreamMessage_pb2'
  # @@protoc_insertion_point(class_scope:LLVM.TDG.StaticStreamInfo)
  ))
_sym_db.RegisterMessage(StaticStreamInfo)

DynamicStreamInfo = _reflection.GeneratedProtocolMessageType('DynamicStreamInfo', (_message.Message,), dict(
  DESCRIPTOR = _DYNAMICSTREAMINFO,
  __module__ = 'StreamMessage_pb2'
  # @@protoc_insertion_point(class_scope:LLVM.TDG.DynamicStreamInfo)
  ))
_sym_db.RegisterMessage(DynamicStreamInfo)

StreamInfo = _reflection.GeneratedProtocolMessageType('StreamInfo', (_message.Message,), dict(

  StreamId = _reflection.GeneratedProtocolMessageType('StreamId', (_message.Message,), dict(
    DESCRIPTOR = _STREAMINFO_STREAMID,
    __module__ = 'StreamMessage_pb2'
    # @@protoc_insertion_point(class_scope:LLVM.TDG.StreamInfo.StreamId)
    ))
  ,
  DESCRIPTOR = _STREAMINFO,
  __module__ = 'StreamMessage_pb2'
  # @@protoc_insertion_point(class_scope:LLVM.TDG.StreamInfo)
  ))
_sym_db.RegisterMessage(StreamInfo)
_sym_db.RegisterMessage(StreamInfo.StreamId)

StreamRegion = _reflection.GeneratedProtocolMessageType('StreamRegion', (_message.Message,), dict(
  DESCRIPTOR = _STREAMREGION,
  __module__ = 'StreamMessage_pb2'
  # @@protoc_insertion_point(class_scope:LLVM.TDG.StreamRegion)
  ))
_sym_db.RegisterMessage(StreamRegion)

StreamPattern = _reflection.GeneratedProtocolMessageType('StreamPattern', (_message.Message,), dict(

  HistoryEntry = _reflection.GeneratedProtocolMessageType('HistoryEntry', (_message.Message,), dict(
    DESCRIPTOR = _STREAMPATTERN_HISTORYENTRY,
    __module__ = 'StreamMessage_pb2'
    # @@protoc_insertion_point(class_scope:LLVM.TDG.StreamPattern.HistoryEntry)
    ))
  ,
  DESCRIPTOR = _STREAMPATTERN,
  __module__ = 'StreamMessage_pb2'
  # @@protoc_insertion_point(class_scope:LLVM.TDG.StreamPattern)
  ))
_sym_db.RegisterMessage(StreamPattern)
_sym_db.RegisterMessage(StreamPattern.HistoryEntry)

StreamHistory = _reflection.GeneratedProtocolMessageType('StreamHistory', (_message.Message,), dict(

  HistoryEntry = _reflection.GeneratedProtocolMessageType('HistoryEntry', (_message.Message,), dict(
    DESCRIPTOR = _STREAMHISTORY_HISTORYENTRY,
    __module__ = 'StreamMessage_pb2'
    # @@protoc_insertion_point(class_scope:LLVM.TDG.StreamHistory.HistoryEntry)
    ))
  ,
  DESCRIPTOR = _STREAMHISTORY,
  __module__ = 'StreamMessage_pb2'
  # @@protoc_insertion_point(class_scope:LLVM.TDG.StreamHistory)
  ))
_sym_db.RegisterMessage(StreamHistory)
_sym_db.RegisterMessage(StreamHistory.HistoryEntry)


# @@protoc_insertion_point(module_scope)
