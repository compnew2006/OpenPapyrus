// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: google/protobuf/unittest_lazy_dependencies_custom_option.proto

#include "google/protobuf/unittest_lazy_dependencies_custom_option.pb.h"
#include <algorithm>

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/extension_set.h>
#include <google/protobuf/wire_format_lite.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/generated_message_reflection.h>
#include <google/protobuf/reflection_ops.h>
#include <google/protobuf/wire_format.h>
// @@protoc_insertion_point(includes)
#include <google/protobuf/port_def.inc>

PROTOBUF_PRAGMA_INIT_SEG
namespace protobuf_unittest {
namespace lazy_imports {
constexpr LazyMessage::LazyMessage(
  ::PROTOBUF_NAMESPACE_ID::internal::ConstantInitialized)
  : a_(0){}
struct LazyMessageDefaultTypeInternal {
	constexpr LazyMessageDefaultTypeInternal() : _instance(::PROTOBUF_NAMESPACE_ID::internal::ConstantInitialized{}) {}
	~LazyMessageDefaultTypeInternal() {}
	union {
		LazyMessage _instance;
	};
};
PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT LazyMessageDefaultTypeInternal _LazyMessage_default_instance_;
}  // namespace lazy_imports
}  // namespace protobuf_unittest
static ::PROTOBUF_NAMESPACE_ID::Metadata file_level_metadata_google_2fprotobuf_2funittest_5flazy_5fdependencies_5fcustom_5foption_2eproto[1];
static constexpr ::PROTOBUF_NAMESPACE_ID::EnumDescriptor const ** file_level_enum_descriptors_google_2fprotobuf_2funittest_5flazy_5fdependencies_5fcustom_5foption_2eproto = nullptr;
static constexpr ::PROTOBUF_NAMESPACE_ID::ServiceDescriptor const ** file_level_service_descriptors_google_2fprotobuf_2funittest_5flazy_5fdependencies_5fcustom_5foption_2eproto = nullptr;

const uint32_t TableStruct_google_2fprotobuf_2funittest_5flazy_5fdependencies_5fcustom_5foption_2eproto::offsets[] PROTOBUF_SECTION_VARIABLE(protodesc_cold) = {
  PROTOBUF_FIELD_OFFSET(::protobuf_unittest::lazy_imports::LazyMessage, _has_bits_),
  PROTOBUF_FIELD_OFFSET(::protobuf_unittest::lazy_imports::LazyMessage, _internal_metadata_),
  ~0u,  // no _extensions_
  ~0u,  // no _oneof_case_
  ~0u,  // no _weak_field_map_
  ~0u,  // no _inlined_string_donated_
  PROTOBUF_FIELD_OFFSET(::protobuf_unittest::lazy_imports::LazyMessage, a_),
  0,
};
static const ::PROTOBUF_NAMESPACE_ID::internal::MigrationSchema schemas[] PROTOBUF_SECTION_VARIABLE(protodesc_cold) = {
  { 0, 7, -1, sizeof(::protobuf_unittest::lazy_imports::LazyMessage)},
};

static ::PROTOBUF_NAMESPACE_ID::Message const * const file_default_instances[] = {
  reinterpret_cast<const ::PROTOBUF_NAMESPACE_ID::Message*>(&::protobuf_unittest::lazy_imports::_LazyMessage_default_instance_),
};

const char descriptor_table_protodef_google_2fprotobuf_2funittest_5flazy_5fdependencies_5fcustom_5foption_2eproto[] PROTOBUF_SECTION_VARIABLE(protodesc_cold) =
  "\n>google/protobuf/unittest_lazy_dependen"
  "cies_custom_option.proto\022\036protobuf_unitt"
  "est.lazy_imports\0325google/protobuf/unitte"
  "st_lazy_dependencies_enum.proto\032 google/"
  "protobuf/descriptor.proto\"\030\n\013LazyMessage"
  "\022\t\n\001a\030\001 \001(\005:s\n\020lazy_enum_option\022\037.google"
  ".protobuf.MessageOptions\030\357\237\213B \001(\0162(.prot"
  "obuf_unittest.lazy_imports.LazyEnum:\013LAZ"
  "Y_ENUM_1B4B$UnittestLazyImportsCustomOpt"
  "ionProtoH\001\200\001\001\210\001\001\220\001\001\370\001\001"
  ;
static const ::PROTOBUF_NAMESPACE_ID::internal::DescriptorTable*const descriptor_table_google_2fprotobuf_2funittest_5flazy_5fdependencies_5fcustom_5foption_2eproto_deps[2] = {
  &::descriptor_table_google_2fprotobuf_2fdescriptor_2eproto,
  &::descriptor_table_google_2fprotobuf_2funittest_5flazy_5fdependencies_5fenum_2eproto,
};
static ::PROTOBUF_NAMESPACE_ID::internal::once_flag descriptor_table_google_2fprotobuf_2funittest_5flazy_5fdependencies_5fcustom_5foption_2eproto_once;
const ::PROTOBUF_NAMESPACE_ID::internal::DescriptorTable descriptor_table_google_2fprotobuf_2funittest_5flazy_5fdependencies_5fcustom_5foption_2eproto = {
  false, false, 382, descriptor_table_protodef_google_2fprotobuf_2funittest_5flazy_5fdependencies_5fcustom_5foption_2eproto, "google/protobuf/unittest_lazy_dependencies_custom_option.proto", 
  &descriptor_table_google_2fprotobuf_2funittest_5flazy_5fdependencies_5fcustom_5foption_2eproto_once, descriptor_table_google_2fprotobuf_2funittest_5flazy_5fdependencies_5fcustom_5foption_2eproto_deps, 2, 1,
  schemas, file_default_instances, TableStruct_google_2fprotobuf_2funittest_5flazy_5fdependencies_5fcustom_5foption_2eproto::offsets,
  file_level_metadata_google_2fprotobuf_2funittest_5flazy_5fdependencies_5fcustom_5foption_2eproto, file_level_enum_descriptors_google_2fprotobuf_2funittest_5flazy_5fdependencies_5fcustom_5foption_2eproto, file_level_service_descriptors_google_2fprotobuf_2funittest_5flazy_5fdependencies_5fcustom_5foption_2eproto,
};
PROTOBUF_ATTRIBUTE_WEAK const ::PROTOBUF_NAMESPACE_ID::internal::DescriptorTable * descriptor_table_google_2fprotobuf_2funittest_5flazy_5fdependencies_5fcustom_5foption_2eproto_getter() { return &descriptor_table_google_2fprotobuf_2funittest_5flazy_5fdependencies_5fcustom_5foption_2eproto; }

// Force running AddDescriptors() at dynamic initialization time.
PROTOBUF_ATTRIBUTE_INIT_PRIORITY static ::PROTOBUF_NAMESPACE_ID::internal::AddDescriptorsRunner dynamic_init_dummy_google_2fprotobuf_2funittest_5flazy_5fdependencies_5fcustom_5foption_2eproto(&descriptor_table_google_2fprotobuf_2funittest_5flazy_5fdependencies_5fcustom_5foption_2eproto);
namespace protobuf_unittest {
namespace lazy_imports {

// ===================================================================

class LazyMessage::_Internal {
 public:
  using HasBits = decltype(std::declval<LazyMessage>()._has_bits_);
  static void set_has_a(HasBits* has_bits) { (*has_bits)[0] |= 1u; }
};

LazyMessage::LazyMessage(::PROTOBUF_NAMESPACE_ID::Arena* arena,
                         bool is_message_owned)
  : ::PROTOBUF_NAMESPACE_ID::Message(arena, is_message_owned) {
  SharedCtor();
  if (!is_message_owned) {
    RegisterArenaDtor(arena);
  }
  // @@protoc_insertion_point(arena_constructor:protobuf_unittest.lazy_imports.LazyMessage)
}
LazyMessage::LazyMessage(const LazyMessage& from)
  : ::PROTOBUF_NAMESPACE_ID::Message(),
      _has_bits_(from._has_bits_) {
  _internal_metadata_.MergeFrom<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(from._internal_metadata_);
  a_ = from.a_;
  // @@protoc_insertion_point(copy_constructor:protobuf_unittest.lazy_imports.LazyMessage)
}

inline void LazyMessage::SharedCtor() {
a_ = 0;
}

LazyMessage::~LazyMessage() {
  // @@protoc_insertion_point(destructor:protobuf_unittest.lazy_imports.LazyMessage)
  if (GetArenaForAllocation() != nullptr) return;
  SharedDtor();
  _internal_metadata_.Delete<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>();
}

inline void LazyMessage::SharedDtor() {
  GOOGLE_DCHECK(GetArenaForAllocation() == nullptr);
}

void LazyMessage::ArenaDtor(void* object) {
  LazyMessage* _this = reinterpret_cast< LazyMessage* >(object);
  (void)_this;
}
void LazyMessage::RegisterArenaDtor(::PROTOBUF_NAMESPACE_ID::Arena*) {
}
void LazyMessage::SetCachedSize(int size) const {
  _cached_size_.Set(size);
}

void LazyMessage::Clear() {
// @@protoc_insertion_point(message_clear_start:protobuf_unittest.lazy_imports.LazyMessage)
  uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  a_ = 0;
  _has_bits_.Clear();
  _internal_metadata_.Clear<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>();
}

const char* LazyMessage::_InternalParse(const char* ptr, ::PROTOBUF_NAMESPACE_ID::internal::ParseContext* ctx) {
#define CHK_(x) if (PROTOBUF_PREDICT_FALSE(!(x))) goto failure
  _Internal::HasBits has_bits{};
  while (!ctx->Done(&ptr)) {
    uint32_t tag;
    ptr = ::PROTOBUF_NAMESPACE_ID::internal::ReadTag(ptr, &tag);
    switch(tag >> 3) {
      // optional int32 a = 1;
      case 1:
        if(PROTOBUF_PREDICT_TRUE(static_cast<uint8_t>(tag) == 8)) {
          _Internal::set_has_a(&has_bits);
          a_ = ::PROTOBUF_NAMESPACE_ID::internal::ReadVarint32(&ptr);
          CHK_(ptr);
        } else
          goto handle_unusual;
        continue;
      default:
        goto handle_unusual;
    }  // switch
  handle_unusual:
    if((tag == 0) || ((tag & 7) == 4)) {
      CHK_(ptr);
      ctx->SetLastTag(tag);
      goto message_done;
    }
    ptr = UnknownFieldParse(
        tag,
        _internal_metadata_.mutable_unknown_fields<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(),
        ptr, ctx);
    CHK_(ptr != nullptr);
  }  // while
message_done:
  _has_bits_.Or(has_bits);
  return ptr;
failure:
  ptr = nullptr;
  goto message_done;
#undef CHK_
}

uint8_t* LazyMessage::_InternalSerialize(
    uint8_t* target, ::PROTOBUF_NAMESPACE_ID::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:protobuf_unittest.lazy_imports.LazyMessage)
  uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  cached_has_bits = _has_bits_[0];
  // optional int32 a = 1;
  if(cached_has_bits & 0x00000001u) {
    target = stream->EnsureSpace(target);
    target = ::PROTOBUF_NAMESPACE_ID::internal::WireFormatLite::WriteInt32ToArray(1, this->_internal_a(), target);
  }

  if(PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target = ::PROTOBUF_NAMESPACE_ID::internal::WireFormat::InternalSerializeUnknownFieldsToArray(
        _internal_metadata_.unknown_fields<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(::PROTOBUF_NAMESPACE_ID::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:protobuf_unittest.lazy_imports.LazyMessage)
  return target;
}

size_t LazyMessage::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:protobuf_unittest.lazy_imports.LazyMessage)
  size_t total_size = 0;

  uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  // optional int32 a = 1;
  cached_has_bits = _has_bits_[0];
  if(cached_has_bits & 0x00000001u) {
    total_size += ::PROTOBUF_NAMESPACE_ID::internal::WireFormatLite::Int32SizePlusOne(this->_internal_a());
  }

  return MaybeComputeUnknownFieldsSize(total_size, &_cached_size_);
}

const ::PROTOBUF_NAMESPACE_ID::Message::ClassData LazyMessage::_class_data_ = {
    ::PROTOBUF_NAMESPACE_ID::Message::CopyWithSizeCheck,
    LazyMessage::MergeImpl
};
const ::PROTOBUF_NAMESPACE_ID::Message::ClassData*LazyMessage::GetClassData() const { return &_class_data_; }

void LazyMessage::MergeImpl(::PROTOBUF_NAMESPACE_ID::Message* to,
                      const ::PROTOBUF_NAMESPACE_ID::Message& from) {
  static_cast<LazyMessage *>(to)->MergeFrom(
      static_cast<const LazyMessage &>(from));
}


void LazyMessage::MergeFrom(const LazyMessage& from) {
// @@protoc_insertion_point(class_specific_merge_from_start:protobuf_unittest.lazy_imports.LazyMessage)
  GOOGLE_DCHECK_NE(&from, this);
  uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if(from._internal_has_a()) {
    _internal_set_a(from._internal_a());
  }
  _internal_metadata_.MergeFrom<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(from._internal_metadata_);
}

void LazyMessage::CopyFrom(const LazyMessage& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:protobuf_unittest.lazy_imports.LazyMessage)
  if(&from == this) return;
  Clear();
  MergeFrom(from);
}

bool LazyMessage::IsInitialized() const {
  return true;
}

void LazyMessage::InternalSwap(LazyMessage* other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  swap(_has_bits_[0], other->_has_bits_[0]);
  swap(a_, other->a_);
}

::PROTOBUF_NAMESPACE_ID::Metadata LazyMessage::GetMetadata() const {
  return ::PROTOBUF_NAMESPACE_ID::internal::AssignDescriptors(&descriptor_table_google_2fprotobuf_2funittest_5flazy_5fdependencies_5fcustom_5foption_2eproto_getter, &descriptor_table_google_2fprotobuf_2funittest_5flazy_5fdependencies_5fcustom_5foption_2eproto_once, file_level_metadata_google_2fprotobuf_2funittest_5flazy_5fdependencies_5fcustom_5foption_2eproto[0]);
}
PROTOBUF_ATTRIBUTE_INIT_PRIORITY ::PROTOBUF_NAMESPACE_ID::internal::ExtensionIdentifier< ::PROTOBUF_NAMESPACE_ID::MessageOptions,
    ::PROTOBUF_NAMESPACE_ID::internal::EnumTypeTraits< ::protobuf_unittest::lazy_imports::LazyEnum, ::protobuf_unittest::lazy_imports::LazyEnum_IsValid>, 14, false >
  lazy_enum_option(kLazyEnumOptionFieldNumber, static_cast< ::protobuf_unittest::lazy_imports::LazyEnum >(1));

// @@protoc_insertion_point(namespace_scope)
}  // namespace lazy_imports
}  // namespace protobuf_unittest
PROTOBUF_NAMESPACE_OPEN
template<> PROTOBUF_NOINLINE ::protobuf_unittest::lazy_imports::LazyMessage* Arena::CreateMaybeMessage< ::protobuf_unittest::lazy_imports::LazyMessage >(Arena* arena) {
  return Arena::CreateMessageInternal< ::protobuf_unittest::lazy_imports::LazyMessage >(arena);
}
PROTOBUF_NAMESPACE_CLOSE

// @@protoc_insertion_point(global_scope)
#include <google/protobuf/port_undef.inc>
