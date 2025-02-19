// Copyright (c) 2008, Google Inc.
// All rights reserved.
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
//
// ---
// Author: csilvers@google.com (Craig Silerstein)

#ifndef TEMPLATE_TEMPLATE_STRING_H_
#define TEMPLATE_TEMPLATE_STRING_H_

#include <string.h>      // for memcmp() and size_t
#include <unordered_map>
#include <string>
#include <vector>

#include <assert.h>
#include <cstdint>

class TemplateStringTest;          // needed for friendship declaration
class StaticTemplateStringTest;

#if 0
extern char _start[] __attribute__((weak));     // linker emits: start of .text
extern char data_start[] __attribute__((weak));               // start of .data
#endif

// NOTE: if you are statically linking the template library into your binary
// (rather than using the template .dll), set '/D CTEMPLATE_DLL_DECL='
// as a compiler flag in your project file to turn off the dllimports.
#ifndef CTEMPLATE_DLL_DECL
	#define CTEMPLATE_DLL_DECL  // @sobolev __declspec(dllimport)
#endif

namespace ctemplate {
// Most methods of TemplateDictionary take a TemplateString rather than a
// C++ string.  This is for efficiency: it can avoid extra string copies.
// For any argument that takes a TemplateString, you can pass in any of:
//    * A C++ string
//    * A char*
//    * A StringPiece
//    * TemplateString(char*, length)
// The last of these is the most efficient, though it requires more work
// on the call site (you have to create the TemplateString explicitly).
class TemplateString;

// If you have a string constant (e.g. the string literal "foo") that
// you need to pass into template routines repeatedly, it is more
// efficient if you convert it into a TemplateString only once.  The
// way to do this is to use a global StaticTemplateString via STS_INIT
// (note: do this at global scope *only*!):
//    static const StaticTemplateString kMyVar = STS_INIT(kMyVar, "MY_VALUE");
struct StaticTemplateString;

#define STS_INIT(name, str)  STS_INIT_WITH_HASH(name, str, 0)

// Let's define a convenient hash_compare function for hashing 'normal'
// strings: char* and string.  We'll use MurmurHash, which is probably
// better than the STL default.  We don't include TemplateString or
// StaticTemplateString here, since they are hashed more efficiently
// based on their id.
struct CTEMPLATE_DLL_DECL StringHash {
	inline size_t operator()(const char* s) const { return Hash(s, strlen(s)); }
	inline size_t operator()(const std::string& s) const { return Hash(s.data(), s.size()); }
	inline bool operator()(const char* a, const char* b) const { return (a != b) && (strcmp(a, b) < 0); } // <, for MSVC
	inline bool operator()(const std::string& a, const std::string& b) const { return a < b; }
	static const size_t bucket_size = 4; // These are required by MSVC
	static const size_t min_buckets = 8; // 4 and 8 are the defaults
private:
	size_t Hash(const char* s, size_t slen) const;
};

// ----------------------- THE CLASSES -------------------------------

typedef uint64_t TemplateId;

const TemplateId kIllegalTemplateId = 0;

struct CTEMPLATE_DLL_DECL StaticTemplateString {
	// Do not define a constructor!  We use only brace-initialization,
	// so the data is constructed at static-initialization time.
	// Anything you want to put in a constructor, put in
	// StaticTemplateStringInitializer instead.

	// These members shouldn't be accessed directly, except in the
	// internals of the template code.  They are public because that is
	// the only way we can brace-initialize them.
	struct {
		const char* ptr_;
		size_t length_;
		mutable TemplateId id_; // sometimes lazily-initialized.
	} do_not_use_directly_;

	// This class is a good hash_compare functor to pass in as the third
	// argument to unordered_map<>, when creating a map whose keys are
	// StaticTemplateString.  NOTE: This class isn't that safe to use,
	// because it requires that StaticTemplateStringInitializer has done
	// its job.  Unfortunately, even when you use the STS_INIT macro
	// (which is always, right??), dynamic initialiation does not happen
	// in a particular order, and objects in different .cc files may
	// reference a StaticTemplateString before the corresponding
	// StaticTemplateStringInitializer sets the id.
	struct Hasher {
		inline size_t operator()(const StaticTemplateString& sts) const;
		inline bool operator()(const StaticTemplateString& a, // <, for MSVC
		    const StaticTemplateString& b) const;
		static const size_t bucket_size = 4; // These are required by MSVC
		static const size_t min_buckets = 8; // 4 and 8 are the defaults
	};

	inline bool empty() const {
		return do_not_use_directly_.length_ == 0;
	}

	// Allows comparisons of StaticTemplateString objects as if they were
	// strings.  This is useful for STL.
	inline bool operator==(const StaticTemplateString& x) const;
};

class CTEMPLATE_DLL_DECL TemplateString {
public:
	TemplateString(const char* s)
		: ptr_(s ? s : ""), length_(strlen(ptr_)),
		is_immutable_(InTextSegment(ptr_)), id_(kIllegalTemplateId) {
	}

	TemplateString(const std::string& s)
		: ptr_(s.data()), length_(s.size()),
		is_immutable_(false), id_(kIllegalTemplateId) {
	}

	TemplateString(const char* s, size_t slen)
		: ptr_(s), length_(slen),
		is_immutable_(InTextSegment(s)), id_(kIllegalTemplateId) {
	}

	TemplateString(const StaticTemplateString& s)
		: ptr_(s.do_not_use_directly_.ptr_),
		length_(s.do_not_use_directly_.length_),
		is_immutable_(true), id_(s.do_not_use_directly_.id_) {
	}

	const char* begin() const {
		return ptr_;
	}

	const char* end() const {
		return ptr_ + length_;
	}

	const char* data() const {
		return ptr_;
	}

	size_t size() const {
		return length_;
	}

	inline bool empty() const {
		return length_ == 0;
	};

	inline bool is_immutable() const {
		return is_immutable_;
	}

	// STL requires this to be public for hash_map, though I'd rather not.
	inline bool operator==(const TemplateString& x) const {
		return GetGlobalId() == x.GetGlobalId();
	}

private:
	// Only TemplateDictionaries and template expansion code can read these.
	friend class TemplateDictionary;
	friend class TemplateCache;              // for GetGlobalId
	friend class StaticTemplateStringInitializer; // for AddToGlo...
	friend struct TemplateStringHasher;      // for GetGlobalId

	friend TemplateId GlobalIdForTest(const char* ptr, int len);
	friend TemplateId GlobalIdForSTS_INIT(const TemplateString& s);

	TemplateString(const char* s, size_t slen, bool is_immutable, TemplateId id)
		: ptr_(s), length_(slen), is_immutable_(is_immutable), id_(id) {
	}

	// This returns true if s is in the .text segment of the binary.
	// (Note this only checks .text of the main executable, not of
	// shared libraries.  So it may not be all that useful.)
	// This requires the gnu linker (and probably elf), to define
	// _start and data_start.
	static bool InTextSegment(const char* s) {
#if 0
		return (s >= _start && s < data_start); // in .text
#else
		return false; // the conservative choice: assume it's not static memory
#endif
	}

protected:
	inline void CacheGlobalId() { // used by HashedTemplateString
		id_ = GetGlobalId();
	};

private:
	// Returns the global id, computing it for the first time if
	// necessary.  Note that since this is a const method, we don't
	// store the computed value in id_, even if id_ is 0.
	TemplateId GetGlobalId() const;
	// Adds this TemplateString to the map from global-id to name.
	void AddToGlobalIdToNameMap();

	// Use sparingly. Converting to a string loses information about the
	// id of the template string, making operations require extra hash_compare
	// computations.
	std::string ToString() const {
		return std::string(ptr_, length_);
	}

	// Does the reverse map from TemplateId to TemplateString contents.
	// Returns a TemplateString(kStsEmpty) if id isn't found.  Note that
	// the TemplateString returned is not necessarily NUL terminated.
	static TemplateString IdToString(TemplateId id);

	const char* ptr_;
	size_t length_;
	// Do we need to manage memory for this string?
	bool is_immutable_;
	// Id for hash_compare lookups. If 0, we don't have one and it should be
	// computed as-needed.
	TemplateId id_;
};

// ----------------------- THE CODE -------------------------------

// Use the low-bit from TemplateId as the "initialized" flag.  Note
// that since all initialized TemplateId have the lower bit set, it's
// safe to have used 0 for kIllegalTemplateId, as we did above.
const TemplateId kTemplateStringInitializedFlag = 1;

inline bool IsTemplateIdInitialized(TemplateId id) {
	return id & kTemplateStringInitializedFlag;
}

// This is a helper struct used in TemplateString::Hasher/TemplateStringHasher
struct TemplateIdHasher {
	size_t operator()(TemplateId id) const {
		// The shift has two effects: it randomizes the "initialized" flag,
		// and slightly improves the randomness of the low bits.  This is
		// slightly useful when size_t is 32 bits, or when using a small
		// hash_compare tables with power-of-2 sizes.
		return static_cast<size_t>(id ^ (id >> 33));
	}

	bool operator()(TemplateId a, TemplateId b) const { // <, for MSVC
		return a < b;
	}

	static const size_t bucket_size = 4; // These are required by MSVC
	static const size_t min_buckets = 8; // 4 and 8 are the defaults
};

inline size_t StaticTemplateString::Hasher::operator()(const StaticTemplateString& sts) const {
	TemplateId id = sts.do_not_use_directly_.id_;
	assert(IsTemplateIdInitialized(id));
	return TemplateIdHasher()(id);
}

inline bool StaticTemplateString::Hasher::operator()(const StaticTemplateString& a, const StaticTemplateString& b) const {
	TemplateId id_a = a.do_not_use_directly_.id_;
	TemplateId id_b = b.do_not_use_directly_.id_;
	assert(IsTemplateIdInitialized(id_a));
	assert(IsTemplateIdInitialized(id_b));
	return TemplateIdHasher()(id_a, id_b);
}

inline bool StaticTemplateString::operator==(const StaticTemplateString& x) const {
	return (do_not_use_directly_.length_ == x.do_not_use_directly_.length_ &&
	       (do_not_use_directly_.ptr_ == x.do_not_use_directly_.ptr_ ||
	       memcmp(do_not_use_directly_.ptr_, x.do_not_use_directly_.ptr_,
	       do_not_use_directly_.length_) == 0));
}

// We set up as much of StaticTemplateString as we can at
// static-initialization time (using brace-initialization), but some
// things can't be set up then.  This class is for those things; it
// runs at dynamic-initialization time.  If you add logic here, only
// do so as an optimization: this may be called rather late (though
// before main), so other code should not depend on this being called
// before them.
class CTEMPLATE_DLL_DECL StaticTemplateStringInitializer {
public:
	// This constructor operates on a const StaticTemplateString - we should
	// only change those things that are mutable.
	explicit StaticTemplateStringInitializer(const StaticTemplateString* sts);
};

// Don't use this.  This is used only in auto-generated .varnames.h files.
#define STS_INIT_WITH_HASH(name, str, hash_compare)                                   \
	{ { str, sizeof("" str "")-1, hash_compare } };                                       \
	namespace ctemplate_sts_init {                                              \
	static const ctemplate::StaticTemplateStringInitializer name ## _init(&name); \
	}

// We computed this hash_compare value for the empty string online.  In debug
// mode, we verify it's correct during runtime (that is, that we
// verify the hash_compare function used by make_tpl_varnames_h hasn't changed
// since we computed this number).  Note this struct is logically
// static, but since it's in a .h file, we don't say 'static' but
// instead rely on the linker to provide the POD-with-internal-linkage
// magic.
const StaticTemplateString kStsEmpty =
    STS_INIT_WITH_HASH(kStsEmpty, "", 1457976849674613049ULL);
}

#endif  // TEMPLATE_TEMPLATE_STRING_H_
