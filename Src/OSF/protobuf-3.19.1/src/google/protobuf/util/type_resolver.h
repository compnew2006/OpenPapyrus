// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// Defines a TypeResolver for the Any message.

#ifndef GOOGLE_PROTOBUF_UTIL_TYPE_RESOLVER_H__
#define GOOGLE_PROTOBUF_UTIL_TYPE_RESOLVER_H__

#include <string>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/type.pb.h>
#include <google/protobuf/stubs/status.h>
#include <google/protobuf/stubs/status.h>
#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {
class DescriptorPool;
namespace util {
// Abstract interface for a type resolver.
//
// Implementations of this interface must be thread-safe.
class PROTOBUF_EXPORT TypeResolver {
public:
	TypeResolver() 
	{
	}
	virtual ~TypeResolver() 
	{
	}
	// Resolves a type url for a message type.
	virtual util::Status ResolveMessageType(const std::string & type_url, google::protobuf::Type* message_type) = 0;
	// Resolves a type url for an enum type.
	virtual util::Status ResolveEnumType(const std::string & type_url, google::protobuf::Enum* enum_type) = 0;
private:
	GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(TypeResolver);
};
}  // namespace util
}  // namespace protobuf
}  // namespace google

#include <google/protobuf/port_undef.inc>

#endif  // GOOGLE_PROTOBUF_UTIL_TYPE_RESOLVER_H__
