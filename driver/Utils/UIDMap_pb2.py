# Generated by the protocol buffer compiler.  DO NOT EDIT!
# source: UIDMap.proto

import sys
_b=sys.version_info[0]<3 and (lambda x:x) or (lambda x:x.encode('latin1'))
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from google.protobuf import reflection as _reflection
from google.protobuf import symbol_database as _symbol_database
from google.protobuf import descriptor_pb2
# @@protoc_insertion_point(imports)

_sym_db = _symbol_database.Default()




DESCRIPTOR = _descriptor.FileDescriptor(
  name='UIDMap.proto',
  package='LLVM.TDG',
  syntax='proto3',
  serialized_pb=_b('\n\x0cUIDMap.proto\x12\x08LLVM.TDG\"?\n\x1aInstructionValueDescriptor\x12\x10\n\x08is_param\x18\x01 \x01(\x08\x12\x0f\n\x07type_id\x18\x02 \x01(\r\"\x86\x01\n\x15InstructionDescriptor\x12\n\n\x02op\x18\x02 \x01(\t\x12\x0c\n\x04\x66unc\x18\x03 \x01(\t\x12\n\n\x02\x62\x62\x18\x04 \x01(\t\x12\x11\n\tpos_in_bb\x18\x05 \x01(\r\x12\x34\n\x06values\x18\x06 \x03(\x0b\x32$.LLVM.TDG.InstructionValueDescriptor\"\'\n\x12\x46unctionDescriptor\x12\x11\n\tfunc_name\x18\x02 \x01(\t\"\x9e\x02\n\x06UIDMap\x12\x13\n\x0bmodule_name\x18\x01 \x01(\t\x12/\n\x08inst_map\x18\x02 \x03(\x0b\x32\x1d.LLVM.TDG.UIDMap.InstMapEntry\x12/\n\x08\x66unc_map\x18\x03 \x03(\x0b\x32\x1d.LLVM.TDG.UIDMap.FuncMapEntry\x1aO\n\x0cInstMapEntry\x12\x0b\n\x03key\x18\x01 \x01(\x04\x12.\n\x05value\x18\x02 \x01(\x0b\x32\x1f.LLVM.TDG.InstructionDescriptor:\x02\x38\x01\x1aL\n\x0c\x46uncMapEntry\x12\x0b\n\x03key\x18\x01 \x01(\t\x12+\n\x05value\x18\x02 \x01(\x0b\x32\x1c.LLVM.TDG.FunctionDescriptor:\x02\x38\x01\x62\x06proto3')
)




_INSTRUCTIONVALUEDESCRIPTOR = _descriptor.Descriptor(
  name='InstructionValueDescriptor',
  full_name='LLVM.TDG.InstructionValueDescriptor',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='is_param', full_name='LLVM.TDG.InstructionValueDescriptor.is_param', index=0,
      number=1, type=8, cpp_type=7, label=1,
      has_default_value=False, default_value=False,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='type_id', full_name='LLVM.TDG.InstructionValueDescriptor.type_id', index=1,
      number=2, type=13, cpp_type=3, label=1,
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
  serialized_start=26,
  serialized_end=89,
)


_INSTRUCTIONDESCRIPTOR = _descriptor.Descriptor(
  name='InstructionDescriptor',
  full_name='LLVM.TDG.InstructionDescriptor',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='op', full_name='LLVM.TDG.InstructionDescriptor.op', index=0,
      number=2, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=_b("").decode('utf-8'),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='func', full_name='LLVM.TDG.InstructionDescriptor.func', index=1,
      number=3, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=_b("").decode('utf-8'),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='bb', full_name='LLVM.TDG.InstructionDescriptor.bb', index=2,
      number=4, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=_b("").decode('utf-8'),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='pos_in_bb', full_name='LLVM.TDG.InstructionDescriptor.pos_in_bb', index=3,
      number=5, type=13, cpp_type=3, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='values', full_name='LLVM.TDG.InstructionDescriptor.values', index=4,
      number=6, type=11, cpp_type=10, label=3,
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
  serialized_start=92,
  serialized_end=226,
)


_FUNCTIONDESCRIPTOR = _descriptor.Descriptor(
  name='FunctionDescriptor',
  full_name='LLVM.TDG.FunctionDescriptor',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='func_name', full_name='LLVM.TDG.FunctionDescriptor.func_name', index=0,
      number=2, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=_b("").decode('utf-8'),
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
  serialized_start=228,
  serialized_end=267,
)


_UIDMAP_INSTMAPENTRY = _descriptor.Descriptor(
  name='InstMapEntry',
  full_name='LLVM.TDG.UIDMap.InstMapEntry',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='key', full_name='LLVM.TDG.UIDMap.InstMapEntry.key', index=0,
      number=1, type=4, cpp_type=4, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='value', full_name='LLVM.TDG.UIDMap.InstMapEntry.value', index=1,
      number=2, type=11, cpp_type=10, label=1,
      has_default_value=False, default_value=None,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
  ],
  options=_descriptor._ParseOptions(descriptor_pb2.MessageOptions(), _b('8\001')),
  is_extendable=False,
  syntax='proto3',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=399,
  serialized_end=478,
)

_UIDMAP_FUNCMAPENTRY = _descriptor.Descriptor(
  name='FuncMapEntry',
  full_name='LLVM.TDG.UIDMap.FuncMapEntry',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='key', full_name='LLVM.TDG.UIDMap.FuncMapEntry.key', index=0,
      number=1, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=_b("").decode('utf-8'),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='value', full_name='LLVM.TDG.UIDMap.FuncMapEntry.value', index=1,
      number=2, type=11, cpp_type=10, label=1,
      has_default_value=False, default_value=None,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
  ],
  options=_descriptor._ParseOptions(descriptor_pb2.MessageOptions(), _b('8\001')),
  is_extendable=False,
  syntax='proto3',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=480,
  serialized_end=556,
)

_UIDMAP = _descriptor.Descriptor(
  name='UIDMap',
  full_name='LLVM.TDG.UIDMap',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='module_name', full_name='LLVM.TDG.UIDMap.module_name', index=0,
      number=1, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=_b("").decode('utf-8'),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='inst_map', full_name='LLVM.TDG.UIDMap.inst_map', index=1,
      number=2, type=11, cpp_type=10, label=3,
      has_default_value=False, default_value=[],
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='func_map', full_name='LLVM.TDG.UIDMap.func_map', index=2,
      number=3, type=11, cpp_type=10, label=3,
      has_default_value=False, default_value=[],
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None, file=DESCRIPTOR),
  ],
  extensions=[
  ],
  nested_types=[_UIDMAP_INSTMAPENTRY, _UIDMAP_FUNCMAPENTRY, ],
  enum_types=[
  ],
  options=None,
  is_extendable=False,
  syntax='proto3',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=270,
  serialized_end=556,
)

_INSTRUCTIONDESCRIPTOR.fields_by_name['values'].message_type = _INSTRUCTIONVALUEDESCRIPTOR
_UIDMAP_INSTMAPENTRY.fields_by_name['value'].message_type = _INSTRUCTIONDESCRIPTOR
_UIDMAP_INSTMAPENTRY.containing_type = _UIDMAP
_UIDMAP_FUNCMAPENTRY.fields_by_name['value'].message_type = _FUNCTIONDESCRIPTOR
_UIDMAP_FUNCMAPENTRY.containing_type = _UIDMAP
_UIDMAP.fields_by_name['inst_map'].message_type = _UIDMAP_INSTMAPENTRY
_UIDMAP.fields_by_name['func_map'].message_type = _UIDMAP_FUNCMAPENTRY
DESCRIPTOR.message_types_by_name['InstructionValueDescriptor'] = _INSTRUCTIONVALUEDESCRIPTOR
DESCRIPTOR.message_types_by_name['InstructionDescriptor'] = _INSTRUCTIONDESCRIPTOR
DESCRIPTOR.message_types_by_name['FunctionDescriptor'] = _FUNCTIONDESCRIPTOR
DESCRIPTOR.message_types_by_name['UIDMap'] = _UIDMAP
_sym_db.RegisterFileDescriptor(DESCRIPTOR)

InstructionValueDescriptor = _reflection.GeneratedProtocolMessageType('InstructionValueDescriptor', (_message.Message,), dict(
  DESCRIPTOR = _INSTRUCTIONVALUEDESCRIPTOR,
  __module__ = 'UIDMap_pb2'
  # @@protoc_insertion_point(class_scope:LLVM.TDG.InstructionValueDescriptor)
  ))
_sym_db.RegisterMessage(InstructionValueDescriptor)

InstructionDescriptor = _reflection.GeneratedProtocolMessageType('InstructionDescriptor', (_message.Message,), dict(
  DESCRIPTOR = _INSTRUCTIONDESCRIPTOR,
  __module__ = 'UIDMap_pb2'
  # @@protoc_insertion_point(class_scope:LLVM.TDG.InstructionDescriptor)
  ))
_sym_db.RegisterMessage(InstructionDescriptor)

FunctionDescriptor = _reflection.GeneratedProtocolMessageType('FunctionDescriptor', (_message.Message,), dict(
  DESCRIPTOR = _FUNCTIONDESCRIPTOR,
  __module__ = 'UIDMap_pb2'
  # @@protoc_insertion_point(class_scope:LLVM.TDG.FunctionDescriptor)
  ))
_sym_db.RegisterMessage(FunctionDescriptor)

UIDMap = _reflection.GeneratedProtocolMessageType('UIDMap', (_message.Message,), dict(

  InstMapEntry = _reflection.GeneratedProtocolMessageType('InstMapEntry', (_message.Message,), dict(
    DESCRIPTOR = _UIDMAP_INSTMAPENTRY,
    __module__ = 'UIDMap_pb2'
    # @@protoc_insertion_point(class_scope:LLVM.TDG.UIDMap.InstMapEntry)
    ))
  ,

  FuncMapEntry = _reflection.GeneratedProtocolMessageType('FuncMapEntry', (_message.Message,), dict(
    DESCRIPTOR = _UIDMAP_FUNCMAPENTRY,
    __module__ = 'UIDMap_pb2'
    # @@protoc_insertion_point(class_scope:LLVM.TDG.UIDMap.FuncMapEntry)
    ))
  ,
  DESCRIPTOR = _UIDMAP,
  __module__ = 'UIDMap_pb2'
  # @@protoc_insertion_point(class_scope:LLVM.TDG.UIDMap)
  ))
_sym_db.RegisterMessage(UIDMap)
_sym_db.RegisterMessage(UIDMap.InstMapEntry)
_sym_db.RegisterMessage(UIDMap.FuncMapEntry)


_UIDMAP_INSTMAPENTRY.has_options = True
_UIDMAP_INSTMAPENTRY._options = _descriptor._ParseOptions(descriptor_pb2.MessageOptions(), _b('8\001'))
_UIDMAP_FUNCMAPENTRY.has_options = True
_UIDMAP_FUNCMAPENTRY._options = _descriptor._ParseOptions(descriptor_pb2.MessageOptions(), _b('8\001'))
# @@protoc_insertion_point(module_scope)