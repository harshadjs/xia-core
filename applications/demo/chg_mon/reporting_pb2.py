# Generated by the protocol buffer compiler.  DO NOT EDIT!
# source: reporting.proto

from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from google.protobuf import reflection as _reflection
from google.protobuf import descriptor_pb2
# @@protoc_insertion_point(imports)




DESCRIPTOR = _descriptor.FileDescriptor(
  name='reporting.proto',
  package='',
  serialized_pb='\n\x0freporting.proto\"\x93\x01\n\nAddrChange\x12\x0e\n\x06intent\x18\x01 \x02(\t\x12\x0e\n\x06newdag\x18\x02 \x02(\t\x12\x0e\n\x06whoami\x18\x03 \x01(\t\x12\"\n\x06reason\x18\x04 \x01(\x0e\x32\x12.AddrChange.Reason\"1\n\x06Reason\x12\x0b\n\x07REFRESH\x10\x00\x12\x0c\n\x08\x45XPLICIT\x10\x01\x12\x0c\n\x08MIGRATED\x10\x02')



_ADDRCHANGE_REASON = _descriptor.EnumDescriptor(
  name='Reason',
  full_name='AddrChange.Reason',
  filename=None,
  file=DESCRIPTOR,
  values=[
    _descriptor.EnumValueDescriptor(
      name='REFRESH', index=0, number=0,
      options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='EXPLICIT', index=1, number=1,
      options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='MIGRATED', index=2, number=2,
      options=None,
      type=None),
  ],
  containing_type=None,
  options=None,
  serialized_start=118,
  serialized_end=167,
)


_ADDRCHANGE = _descriptor.Descriptor(
  name='AddrChange',
  full_name='AddrChange',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='intent', full_name='AddrChange.intent', index=0,
      number=1, type=9, cpp_type=9, label=2,
      has_default_value=False, default_value=unicode("", "utf-8"),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None),
    _descriptor.FieldDescriptor(
      name='newdag', full_name='AddrChange.newdag', index=1,
      number=2, type=9, cpp_type=9, label=2,
      has_default_value=False, default_value=unicode("", "utf-8"),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None),
    _descriptor.FieldDescriptor(
      name='whoami', full_name='AddrChange.whoami', index=2,
      number=3, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=unicode("", "utf-8"),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None),
    _descriptor.FieldDescriptor(
      name='reason', full_name='AddrChange.reason', index=3,
      number=4, type=14, cpp_type=8, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
    _ADDRCHANGE_REASON,
  ],
  options=None,
  is_extendable=False,
  extension_ranges=[],
  serialized_start=20,
  serialized_end=167,
)

_ADDRCHANGE.fields_by_name['reason'].enum_type = _ADDRCHANGE_REASON
_ADDRCHANGE_REASON.containing_type = _ADDRCHANGE;
DESCRIPTOR.message_types_by_name['AddrChange'] = _ADDRCHANGE

class AddrChange(_message.Message):
  __metaclass__ = _reflection.GeneratedProtocolMessageType
  DESCRIPTOR = _ADDRCHANGE

  # @@protoc_insertion_point(class_scope:AddrChange)


# @@protoc_insertion_point(module_scope)
