// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: google/protobuf/unittest_mset_wire_format.proto

#include "google/protobuf/unittest_mset_wire_format.pb.h"
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
namespace proto2_wireformat_unittest {
constexpr TestMessageSet::TestMessageSet(
  ::PROTOBUF_NAMESPACE_ID::internal::ConstantInitialized){}
struct TestMessageSetDefaultTypeInternal {
	constexpr TestMessageSetDefaultTypeInternal() : _instance(::PROTOBUF_NAMESPACE_ID::internal::ConstantInitialized{}) {}
	~TestMessageSetDefaultTypeInternal() {}
	union {
		TestMessageSet _instance;
	};
};
PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT TestMessageSetDefaultTypeInternal _TestMessageSet_default_instance_;
constexpr TestMessageSetWireFormatContainer::TestMessageSetWireFormatContainer(
  ::PROTOBUF_NAMESPACE_ID::internal::ConstantInitialized)
  : message_set_(nullptr){}
struct TestMessageSetWireFormatContainerDefaultTypeInternal {
	constexpr TestMessageSetWireFormatContainerDefaultTypeInternal() : _instance(::PROTOBUF_NAMESPACE_ID::internal::ConstantInitialized{}) {}
	~TestMessageSetWireFormatContainerDefaultTypeInternal() {}
	union {
		TestMessageSetWireFormatContainer _instance;
	};
};
PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT TestMessageSetWireFormatContainerDefaultTypeInternal _TestMessageSetWireFormatContainer_default_instance_;
}  // namespace proto2_wireformat_unittest
static ::PROTOBUF_NAMESPACE_ID::Metadata file_level_metadata_google_2fprotobuf_2funittest_5fmset_5fwire_5fformat_2eproto[2];
static constexpr ::PROTOBUF_NAMESPACE_ID::EnumDescriptor const ** file_level_enum_descriptors_google_2fprotobuf_2funittest_5fmset_5fwire_5fformat_2eproto = nullptr;
static constexpr ::PROTOBUF_NAMESPACE_ID::ServiceDescriptor const ** file_level_service_descriptors_google_2fprotobuf_2funittest_5fmset_5fwire_5fformat_2eproto = nullptr;

const uint32_t TableStruct_google_2fprotobuf_2funittest_5fmset_5fwire_5fformat_2eproto::offsets[] PROTOBUF_SECTION_VARIABLE(protodesc_cold) = {
  ~0u,  // no _has_bits_
  PROTOBUF_FIELD_OFFSET(::proto2_wireformat_unittest::TestMessageSet, _internal_metadata_),
  PROTOBUF_FIELD_OFFSET(::proto2_wireformat_unittest::TestMessageSet, _extensions_),
  ~0u,  // no _oneof_case_
  ~0u,  // no _weak_field_map_
  ~0u,  // no _inlined_string_donated_
  PROTOBUF_FIELD_OFFSET(::proto2_wireformat_unittest::TestMessageSetWireFormatContainer, _has_bits_),
  PROTOBUF_FIELD_OFFSET(::proto2_wireformat_unittest::TestMessageSetWireFormatContainer, _internal_metadata_),
  ~0u,  // no _extensions_
  ~0u,  // no _oneof_case_
  ~0u,  // no _weak_field_map_
  ~0u,  // no _inlined_string_donated_
  PROTOBUF_FIELD_OFFSET(::proto2_wireformat_unittest::TestMessageSetWireFormatContainer, message_set_),
  0,
};
static const ::PROTOBUF_NAMESPACE_ID::internal::MigrationSchema schemas[] PROTOBUF_SECTION_VARIABLE(protodesc_cold) = {
  { 0, -1, -1, sizeof(::proto2_wireformat_unittest::TestMessageSet)},
  { 6, 13, -1, sizeof(::proto2_wireformat_unittest::TestMessageSetWireFormatContainer)},
};

static ::PROTOBUF_NAMESPACE_ID::Message const * const file_default_instances[] = {
  reinterpret_cast<const ::PROTOBUF_NAMESPACE_ID::Message*>(&::proto2_wireformat_unittest::_TestMessageSet_default_instance_),
  reinterpret_cast<const ::PROTOBUF_NAMESPACE_ID::Message*>(&::proto2_wireformat_unittest::_TestMessageSetWireFormatContainer_default_instance_),
};

const char descriptor_table_protodef_google_2fprotobuf_2funittest_5fmset_5fwire_5fformat_2eproto[] PROTOBUF_SECTION_VARIABLE(protodesc_cold) =
  "\n/google/protobuf/unittest_mset_wire_for"
  "mat.proto\022\032proto2_wireformat_unittest\"\036\n"
  "\016TestMessageSet*\010\010\004\020\377\377\377\377\007:\002\010\001\"d\n!TestMes"
  "sageSetWireFormatContainer\022\?\n\013message_se"
  "t\030\001 \001(\0132*.proto2_wireformat_unittest.Tes"
  "tMessageSetB)H\001\370\001\001\252\002!Google.ProtocolBuff"
  "ers.TestProtos"
  ;
static ::PROTOBUF_NAMESPACE_ID::internal::once_flag descriptor_table_google_2fprotobuf_2funittest_5fmset_5fwire_5fformat_2eproto_once;
const ::PROTOBUF_NAMESPACE_ID::internal::DescriptorTable descriptor_table_google_2fprotobuf_2funittest_5fmset_5fwire_5fformat_2eproto = {
  false, false, 254, descriptor_table_protodef_google_2fprotobuf_2funittest_5fmset_5fwire_5fformat_2eproto, "google/protobuf/unittest_mset_wire_format.proto", 
  &descriptor_table_google_2fprotobuf_2funittest_5fmset_5fwire_5fformat_2eproto_once, nullptr, 0, 2,
  schemas, file_default_instances, TableStruct_google_2fprotobuf_2funittest_5fmset_5fwire_5fformat_2eproto::offsets,
  file_level_metadata_google_2fprotobuf_2funittest_5fmset_5fwire_5fformat_2eproto, file_level_enum_descriptors_google_2fprotobuf_2funittest_5fmset_5fwire_5fformat_2eproto, file_level_service_descriptors_google_2fprotobuf_2funittest_5fmset_5fwire_5fformat_2eproto,
};
PROTOBUF_ATTRIBUTE_WEAK const ::PROTOBUF_NAMESPACE_ID::internal::DescriptorTable * descriptor_table_google_2fprotobuf_2funittest_5fmset_5fwire_5fformat_2eproto_getter() { return &descriptor_table_google_2fprotobuf_2funittest_5fmset_5fwire_5fformat_2eproto; }

// Force running AddDescriptors() at dynamic initialization time.
PROTOBUF_ATTRIBUTE_INIT_PRIORITY static ::PROTOBUF_NAMESPACE_ID::internal::AddDescriptorsRunner dynamic_init_dummy_google_2fprotobuf_2funittest_5fmset_5fwire_5fformat_2eproto(&descriptor_table_google_2fprotobuf_2funittest_5fmset_5fwire_5fformat_2eproto);
namespace proto2_wireformat_unittest {

// ===================================================================

class TestMessageSet::_Internal {
 public:
};

TestMessageSet::TestMessageSet(::PROTOBUF_NAMESPACE_ID::Arena* arena,
                         bool is_message_owned)
  : ::PROTOBUF_NAMESPACE_ID::Message(arena, is_message_owned),
  _extensions_(arena) {
  SharedCtor();
  if (!is_message_owned) {
    RegisterArenaDtor(arena);
  }
  // @@protoc_insertion_point(arena_constructor:proto2_wireformat_unittest.TestMessageSet)
}
TestMessageSet::TestMessageSet(const TestMessageSet& from)
  : ::PROTOBUF_NAMESPACE_ID::Message() {
  _internal_metadata_.MergeFrom<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(from._internal_metadata_);
  _extensions_.MergeFrom(internal_default_instance(), from._extensions_);
  // @@protoc_insertion_point(copy_constructor:proto2_wireformat_unittest.TestMessageSet)
}

inline void TestMessageSet::SharedCtor() {
}

TestMessageSet::~TestMessageSet() {
  // @@protoc_insertion_point(destructor:proto2_wireformat_unittest.TestMessageSet)
  if (GetArenaForAllocation() != nullptr) return;
  SharedDtor();
  _internal_metadata_.Delete<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>();
}

inline void TestMessageSet::SharedDtor() {
  GOOGLE_DCHECK(GetArenaForAllocation() == nullptr);
}

void TestMessageSet::ArenaDtor(void* object) {
  TestMessageSet* _this = reinterpret_cast< TestMessageSet* >(object);
  (void)_this;
}
void TestMessageSet::RegisterArenaDtor(::PROTOBUF_NAMESPACE_ID::Arena*) {
}
void TestMessageSet::SetCachedSize(int size) const {
  _cached_size_.Set(size);
}

void TestMessageSet::Clear() {
// @@protoc_insertion_point(message_clear_start:proto2_wireformat_unittest.TestMessageSet)
  uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _extensions_.Clear();
  _internal_metadata_.Clear<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>();
}

const char* TestMessageSet::_InternalParse(const char* ptr,
                  ::PROTOBUF_NAMESPACE_ID::internal::ParseContext* ctx) {
  return _extensions_.ParseMessageSet(ptr, 
      internal_default_instance(), &_internal_metadata_, ctx);
}

uint8_t* TestMessageSet::_InternalSerialize(
    uint8_t* target, ::PROTOBUF_NAMESPACE_ID::io::EpsCopyOutputStream* stream) const {
  target = _extensions_.InternalSerializeMessageSetWithCachedSizesToArray(
internal_default_instance(), target, stream);
  target = ::PROTOBUF_NAMESPACE_ID::internal::InternalSerializeUnknownMessageSetItemsToArray(
               _internal_metadata_.unknown_fields<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(::PROTOBUF_NAMESPACE_ID::UnknownFieldSet::default_instance), target, stream);
  return target;
}

size_t TestMessageSet::ByteSizeLong() const {
// @@protoc_insertion_point(message_set_byte_size_start:proto2_wireformat_unittest.TestMessageSet)
  size_t total_size = _extensions_.MessageSetByteSize();
  if(_internal_metadata_.have_unknown_fields()) {
    total_size += ::PROTOBUF_NAMESPACE_ID::internal::ComputeUnknownMessageSetItemsSize(_internal_metadata_.unknown_fields<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(::PROTOBUF_NAMESPACE_ID::UnknownFieldSet::default_instance));
  }
  int cached_size = ::PROTOBUF_NAMESPACE_ID::internal::ToCachedSize(total_size);
  SetCachedSize(cached_size);
  return total_size;
}

const ::PROTOBUF_NAMESPACE_ID::Message::ClassData TestMessageSet::_class_data_ = {
    ::PROTOBUF_NAMESPACE_ID::Message::CopyWithSizeCheck,
    TestMessageSet::MergeImpl
};
const ::PROTOBUF_NAMESPACE_ID::Message::ClassData*TestMessageSet::GetClassData() const { return &_class_data_; }

void TestMessageSet::MergeImpl(::PROTOBUF_NAMESPACE_ID::Message* to,
                      const ::PROTOBUF_NAMESPACE_ID::Message& from) {
  static_cast<TestMessageSet *>(to)->MergeFrom(
      static_cast<const TestMessageSet &>(from));
}


void TestMessageSet::MergeFrom(const TestMessageSet& from) {
// @@protoc_insertion_point(class_specific_merge_from_start:proto2_wireformat_unittest.TestMessageSet)
  GOOGLE_DCHECK_NE(&from, this);
  uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  _extensions_.MergeFrom(internal_default_instance(), from._extensions_);
  _internal_metadata_.MergeFrom<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(from._internal_metadata_);
}

void TestMessageSet::CopyFrom(const TestMessageSet& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:proto2_wireformat_unittest.TestMessageSet)
  if(&from == this) return;
  Clear();
  MergeFrom(from);
}

bool TestMessageSet::IsInitialized() const {
  if(!_extensions_.IsInitialized()) {
    return false;
  }

  return true;
}

void TestMessageSet::InternalSwap(TestMessageSet* other) {
  using std::swap;
  _extensions_.InternalSwap(&other->_extensions_);
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
}

::PROTOBUF_NAMESPACE_ID::Metadata TestMessageSet::GetMetadata() const {
  return ::PROTOBUF_NAMESPACE_ID::internal::AssignDescriptors(&descriptor_table_google_2fprotobuf_2funittest_5fmset_5fwire_5fformat_2eproto_getter, &descriptor_table_google_2fprotobuf_2funittest_5fmset_5fwire_5fformat_2eproto_once, file_level_metadata_google_2fprotobuf_2funittest_5fmset_5fwire_5fformat_2eproto[0]);
}

// ===================================================================

class TestMessageSetWireFormatContainer::_Internal {
 public:
  using HasBits = decltype(std::declval<TestMessageSetWireFormatContainer>()._has_bits_);
  static const ::proto2_wireformat_unittest::TestMessageSet& message_set(const TestMessageSetWireFormatContainer* msg);
  static void set_has_message_set(HasBits* has_bits) { (*has_bits)[0] |= 1u; }
};

const ::proto2_wireformat_unittest::TestMessageSet&
TestMessageSetWireFormatContainer::_Internal::message_set(const TestMessageSetWireFormatContainer* msg) {
  return *msg->message_set_;
}
TestMessageSetWireFormatContainer::TestMessageSetWireFormatContainer(::PROTOBUF_NAMESPACE_ID::Arena* arena,
                         bool is_message_owned)
  : ::PROTOBUF_NAMESPACE_ID::Message(arena, is_message_owned) {
  SharedCtor();
  if (!is_message_owned) {
    RegisterArenaDtor(arena);
  }
  // @@protoc_insertion_point(arena_constructor:proto2_wireformat_unittest.TestMessageSetWireFormatContainer)
}
TestMessageSetWireFormatContainer::TestMessageSetWireFormatContainer(const TestMessageSetWireFormatContainer& from)
  : ::PROTOBUF_NAMESPACE_ID::Message(),
      _has_bits_(from._has_bits_) {
  _internal_metadata_.MergeFrom<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(from._internal_metadata_);
  if(from._internal_has_message_set()) {
    message_set_ = new ::proto2_wireformat_unittest::TestMessageSet(*from.message_set_);
  } else {
    message_set_ = nullptr;
  }
  // @@protoc_insertion_point(copy_constructor:proto2_wireformat_unittest.TestMessageSetWireFormatContainer)
}

inline void TestMessageSetWireFormatContainer::SharedCtor() {
message_set_ = nullptr;
}

TestMessageSetWireFormatContainer::~TestMessageSetWireFormatContainer() {
  // @@protoc_insertion_point(destructor:proto2_wireformat_unittest.TestMessageSetWireFormatContainer)
  if (GetArenaForAllocation() != nullptr) return;
  SharedDtor();
  _internal_metadata_.Delete<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>();
}

inline void TestMessageSetWireFormatContainer::SharedDtor() {
  GOOGLE_DCHECK(GetArenaForAllocation() == nullptr);
  if(this != internal_default_instance()) delete message_set_;
}

void TestMessageSetWireFormatContainer::ArenaDtor(void* object) {
  TestMessageSetWireFormatContainer* _this = reinterpret_cast< TestMessageSetWireFormatContainer* >(object);
  (void)_this;
}
void TestMessageSetWireFormatContainer::RegisterArenaDtor(::PROTOBUF_NAMESPACE_ID::Arena*) {
}
void TestMessageSetWireFormatContainer::SetCachedSize(int size) const {
  _cached_size_.Set(size);
}

void TestMessageSetWireFormatContainer::Clear() {
// @@protoc_insertion_point(message_clear_start:proto2_wireformat_unittest.TestMessageSetWireFormatContainer)
  uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  cached_has_bits = _has_bits_[0];
  if(cached_has_bits & 0x00000001u) {
    GOOGLE_DCHECK(message_set_ != nullptr);
    message_set_->Clear();
  }
  _has_bits_.Clear();
  _internal_metadata_.Clear<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>();
}

const char* TestMessageSetWireFormatContainer::_InternalParse(const char* ptr, ::PROTOBUF_NAMESPACE_ID::internal::ParseContext* ctx) {
#define CHK_(x) if (PROTOBUF_PREDICT_FALSE(!(x))) goto failure
  _Internal::HasBits has_bits{};
  while (!ctx->Done(&ptr)) {
    uint32_t tag;
    ptr = ::PROTOBUF_NAMESPACE_ID::internal::ReadTag(ptr, &tag);
    switch(tag >> 3) {
      // optional .proto2_wireformat_unittest.TestMessageSet message_set = 1;
      case 1:
        if(PROTOBUF_PREDICT_TRUE(static_cast<uint8_t>(tag) == 10)) {
          ptr = ctx->ParseMessage(_internal_mutable_message_set(), ptr);
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

uint8_t* TestMessageSetWireFormatContainer::_InternalSerialize(
    uint8_t* target, ::PROTOBUF_NAMESPACE_ID::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:proto2_wireformat_unittest.TestMessageSetWireFormatContainer)
  uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  cached_has_bits = _has_bits_[0];
  // optional .proto2_wireformat_unittest.TestMessageSet message_set = 1;
  if(cached_has_bits & 0x00000001u) {
    target = stream->EnsureSpace(target);
    target = ::PROTOBUF_NAMESPACE_ID::internal::WireFormatLite::
      InternalWriteMessage(
        1, _Internal::message_set(this), target, stream);
  }

  if(PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target = ::PROTOBUF_NAMESPACE_ID::internal::WireFormat::InternalSerializeUnknownFieldsToArray(
        _internal_metadata_.unknown_fields<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(::PROTOBUF_NAMESPACE_ID::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:proto2_wireformat_unittest.TestMessageSetWireFormatContainer)
  return target;
}

size_t TestMessageSetWireFormatContainer::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:proto2_wireformat_unittest.TestMessageSetWireFormatContainer)
  size_t total_size = 0;

  uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  // optional .proto2_wireformat_unittest.TestMessageSet message_set = 1;
  cached_has_bits = _has_bits_[0];
  if(cached_has_bits & 0x00000001u) {
    total_size += 1 +
      ::PROTOBUF_NAMESPACE_ID::internal::WireFormatLite::MessageSize(
        *message_set_);
  }

  return MaybeComputeUnknownFieldsSize(total_size, &_cached_size_);
}

const ::PROTOBUF_NAMESPACE_ID::Message::ClassData TestMessageSetWireFormatContainer::_class_data_ = {
    ::PROTOBUF_NAMESPACE_ID::Message::CopyWithSizeCheck,
    TestMessageSetWireFormatContainer::MergeImpl
};
const ::PROTOBUF_NAMESPACE_ID::Message::ClassData*TestMessageSetWireFormatContainer::GetClassData() const { return &_class_data_; }

void TestMessageSetWireFormatContainer::MergeImpl(::PROTOBUF_NAMESPACE_ID::Message* to,
                      const ::PROTOBUF_NAMESPACE_ID::Message& from) {
  static_cast<TestMessageSetWireFormatContainer *>(to)->MergeFrom(
      static_cast<const TestMessageSetWireFormatContainer &>(from));
}


void TestMessageSetWireFormatContainer::MergeFrom(const TestMessageSetWireFormatContainer& from) {
// @@protoc_insertion_point(class_specific_merge_from_start:proto2_wireformat_unittest.TestMessageSetWireFormatContainer)
  GOOGLE_DCHECK_NE(&from, this);
  uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if(from._internal_has_message_set()) {
    _internal_mutable_message_set()->::proto2_wireformat_unittest::TestMessageSet::MergeFrom(from._internal_message_set());
  }
  _internal_metadata_.MergeFrom<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(from._internal_metadata_);
}

void TestMessageSetWireFormatContainer::CopyFrom(const TestMessageSetWireFormatContainer& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:proto2_wireformat_unittest.TestMessageSetWireFormatContainer)
  if(&from == this) return;
  Clear();
  MergeFrom(from);
}

bool TestMessageSetWireFormatContainer::IsInitialized() const {
  if(_internal_has_message_set()) {
    if (!message_set_->IsInitialized()) return false;
  }
  return true;
}

void TestMessageSetWireFormatContainer::InternalSwap(TestMessageSetWireFormatContainer* other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  swap(_has_bits_[0], other->_has_bits_[0]);
  swap(message_set_, other->message_set_);
}

::PROTOBUF_NAMESPACE_ID::Metadata TestMessageSetWireFormatContainer::GetMetadata() const {
  return ::PROTOBUF_NAMESPACE_ID::internal::AssignDescriptors(&descriptor_table_google_2fprotobuf_2funittest_5fmset_5fwire_5fformat_2eproto_getter, &descriptor_table_google_2fprotobuf_2funittest_5fmset_5fwire_5fformat_2eproto_once, file_level_metadata_google_2fprotobuf_2funittest_5fmset_5fwire_5fformat_2eproto[1]);
}

// @@protoc_insertion_point(namespace_scope)
}  // namespace proto2_wireformat_unittest
PROTOBUF_NAMESPACE_OPEN
template<> PROTOBUF_NOINLINE ::proto2_wireformat_unittest::TestMessageSet* Arena::CreateMaybeMessage< ::proto2_wireformat_unittest::TestMessageSet >(Arena* arena) {
  return Arena::CreateMessageInternal< ::proto2_wireformat_unittest::TestMessageSet >(arena);
}
template<> PROTOBUF_NOINLINE ::proto2_wireformat_unittest::TestMessageSetWireFormatContainer* Arena::CreateMaybeMessage< ::proto2_wireformat_unittest::TestMessageSetWireFormatContainer >(Arena* arena) {
  return Arena::CreateMessageInternal< ::proto2_wireformat_unittest::TestMessageSetWireFormatContainer >(arena);
}
PROTOBUF_NAMESPACE_CLOSE

// @@protoc_insertion_point(global_scope)
#include <google/protobuf/port_undef.inc>
