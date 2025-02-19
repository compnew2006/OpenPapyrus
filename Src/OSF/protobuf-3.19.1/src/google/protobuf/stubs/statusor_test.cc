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
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <protobuf-internal.h>
#pragma hdrstop
#include <google/protobuf/stubs/statusor.h>
#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>

namespace google {
namespace protobuf {
namespace util {
namespace {
class Base1 {
public:
	virtual ~Base1() {
	}

	int pad;
};

class Base2 {
public:
	virtual ~Base2() {
	}

	int yetotherpad;
};

class Derived : public Base1, public Base2 {
public:
	virtual ~Derived() {
	}

	int evenmorepad;
};

class CopyNoAssign {
public:
	explicit CopyNoAssign(int value) : foo(value) {
	}

	CopyNoAssign(const CopyNoAssign& other) : foo(other.foo) {
	}

	int foo;
private:
	const CopyNoAssign& operator=(const CopyNoAssign&);
};

TEST(StatusOr, TestDefaultCtor) {
	StatusOr<int> thing;
	EXPECT_FALSE(thing.ok());
	EXPECT_EQ(util::UnknownError(""), thing.status());
}

TEST(StatusOr, TestStatusCtor) {
	StatusOr<int> thing(util::CancelledError(""));
	EXPECT_FALSE(thing.ok());
	EXPECT_EQ(util::CancelledError(""), thing.status());
}

TEST(StatusOr, TestValueCtor) {
	const int kI = 4;
	StatusOr<int> thing(kI);
	EXPECT_TRUE(thing.ok());
	EXPECT_EQ(kI, thing.value());
}

TEST(StatusOr, TestCopyCtorStatusOk) {
	const int kI = 4;
	StatusOr<int> original(kI);
	StatusOr<int> copy(original);
	EXPECT_EQ(original.status(), copy.status());
	EXPECT_EQ(original.value(), copy.value());
}

TEST(StatusOr, TestCopyCtorStatusNotOk) {
	StatusOr<int> original(util::CancelledError(""));
	StatusOr<int> copy(original);
	EXPECT_EQ(original.status(), copy.status());
}

TEST(StatusOr, TestCopyCtorStatusOKConverting) {
	const int kI = 4;
	StatusOr<int>    original(kI);
	StatusOr<double> copy(original);
	EXPECT_EQ(original.status(), copy.status());
	EXPECT_EQ(original.value(), copy.value());
}

TEST(StatusOr, TestCopyCtorStatusNotOkConverting) {
	StatusOr<int> original(util::CancelledError(""));
	StatusOr<double> copy(original);
	EXPECT_EQ(original.status(), copy.status());
}

TEST(StatusOr, TestAssignmentStatusOk) {
	const int kI = 4;
	StatusOr<int> source(kI);
	StatusOr<int> target;
	target = source;
	EXPECT_EQ(source.status(), target.status());
	EXPECT_EQ(source.value(), target.value());
}

TEST(StatusOr, TestAssignmentStatusNotOk) {
	StatusOr<int> source(util::CancelledError(""));
	StatusOr<int> target;
	target = source;
	EXPECT_EQ(source.status(), target.status());
}

TEST(StatusOr, TestAssignmentStatusOKConverting) {
	const int kI = 4;
	StatusOr<int>    source(kI);
	StatusOr<double> target;
	target = source;
	EXPECT_EQ(source.status(), target.status());
	EXPECT_DOUBLE_EQ(source.value(), target.value());
}

TEST(StatusOr, TestAssignmentStatusNotOkConverting) {
	StatusOr<int> source(util::CancelledError(""));
	StatusOr<double> target;
	target = source;
	EXPECT_EQ(source.status(), target.status());
}

TEST(StatusOr, TestStatus) {
	StatusOr<int> good(4);
	EXPECT_TRUE(good.ok());
	StatusOr<int> bad(util::CancelledError(""));
	EXPECT_FALSE(bad.ok());
	EXPECT_EQ(util::CancelledError(""), bad.status());
}

TEST(StatusOr, TestValue) {
	const int kI = 4;
	StatusOr<int> thing(kI);
	EXPECT_EQ(kI, thing.value());
}

TEST(StatusOr, TestValueConst) {
	const int kI = 4;
	const StatusOr<int> thing(kI);
	EXPECT_EQ(kI, thing.value());
}

TEST(StatusOr, TestPointerDefaultCtor) {
	StatusOr<int*> thing;
	EXPECT_FALSE(thing.ok());
	EXPECT_EQ(util::UnknownError(""), thing.status());
}

TEST(StatusOr, TestPointerStatusCtor) {
	StatusOr<int*> thing(util::CancelledError(""));
	EXPECT_FALSE(thing.ok());
	EXPECT_EQ(util::CancelledError(""), thing.status());
}

TEST(StatusOr, TestPointerValueCtor) {
	const int kI = 4;
	StatusOr<const int*> thing(&kI);
	EXPECT_TRUE(thing.ok());
	EXPECT_EQ(&kI, thing.value());
}

TEST(StatusOr, TestPointerCopyCtorStatusOk) {
	const int kI = 0;
	StatusOr<const int*> original(&kI);
	StatusOr<const int*> copy(original);
	EXPECT_EQ(original.status(), copy.status());
	EXPECT_EQ(original.value(), copy.value());
}

TEST(StatusOr, TestPointerCopyCtorStatusNotOk) {
	StatusOr<int*> original(util::CancelledError(""));
	StatusOr<int*> copy(original);
	EXPECT_EQ(original.status(), copy.status());
}

TEST(StatusOr, TestPointerCopyCtorStatusOKConverting) {
	Derived derived;
	StatusOr<Derived*> original(&derived);
	StatusOr<Base2*>   copy(original);
	EXPECT_EQ(original.status(), copy.status());
	EXPECT_EQ(static_cast<const Base2*>(original.value()), copy.value());
}

TEST(StatusOr, TestPointerCopyCtorStatusNotOkConverting) {
	StatusOr<Derived*> original(util::CancelledError(""));
	StatusOr<Base2*>   copy(original);
	EXPECT_EQ(original.status(), copy.status());
}

TEST(StatusOr, TestPointerAssignmentStatusOk) {
	const int kI = 0;
	StatusOr<const int*> source(&kI);
	StatusOr<const int*> target;
	target = source;
	EXPECT_EQ(source.status(), target.status());
	EXPECT_EQ(source.value(), target.value());
}

TEST(StatusOr, TestPointerAssignmentStatusNotOk) {
	StatusOr<int*> source(util::CancelledError(""));
	StatusOr<int*> target;
	target = source;
	EXPECT_EQ(source.status(), target.status());
}

TEST(StatusOr, TestPointerAssignmentStatusOKConverting) {
	Derived derived;
	StatusOr<Derived*> source(&derived);
	StatusOr<Base2*>   target;
	target = source;
	EXPECT_EQ(source.status(), target.status());
	EXPECT_EQ(static_cast<const Base2*>(source.value()), target.value());
}

TEST(StatusOr, TestPointerAssignmentStatusNotOkConverting) {
	StatusOr<Derived*> source(util::CancelledError(""));
	StatusOr<Base2*>   target;
	target = source;
	EXPECT_EQ(source.status(), target.status());
}

TEST(StatusOr, TestPointerStatus) {
	const int kI = 0;
	StatusOr<const int*> good(&kI);
	EXPECT_TRUE(good.ok());
	StatusOr<const int*> bad(util::CancelledError(""));
	EXPECT_EQ(util::CancelledError(""), bad.status());
}

TEST(StatusOr, TestPointerValue) {
	const int kI = 0;
	StatusOr<const int*> thing(&kI);
	EXPECT_EQ(&kI, thing.value());
}

TEST(StatusOr, TestPointerValueConst) {
	const int kI = 0;
	const StatusOr<const int*> thing(&kI);
	EXPECT_EQ(&kI, thing.value());
}
}  // namespace
}  // namespace util
}  // namespace protobuf
}  // namespace google
