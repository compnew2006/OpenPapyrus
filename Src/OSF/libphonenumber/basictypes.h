// BASICTYPES.H
// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef I18N_PHONENUMBERS_BASE_BASICTYPES_H_
#define I18N_PHONENUMBERS_BASE_BASICTYPES_H_

namespace i18n {
	namespace phonenumbers {
		#ifdef INT64_MAX
			// INT64_MAX is defined if C99 stdint.h is included; use the
			// native types if available.
			// @sobolev typedef int8_t int8;
			// @sobolev typedef int16_t int16;
			typedef int32_t int32;
			// @sobolev typedef int64_t int64;
			// @sobolev typedef uint8_t uint8;
			// @sobolev typedef uint16_t uint16;
			typedef uint32_t uint32;
			// @sobolev typedef uint64_t uint64;

			const uint8  kuint8max  = UINT8_MAX;
			const uint16 kuint16max = UINT16_MAX;
			const uint32 kuint32max = UINT32_MAX;
			const uint64 kuint64max = UINT64_MAX;
			const  int8  kint8min   = INT8_MIN;
			const  int8  kint8max   = INT8_MAX;
			const  int16 kint16min  = INT16_MIN;
			const  int16 kint16max  = INT16_MAX;
			const  int32 kint32min  = INT32_MIN;
			const  int32 kint32max  = INT32_MAX;
			const  int64 kint64min  = INT64_MIN;
			const  int64 kint64max  = INT64_MAX;
		#else // !INT64_MAX
			// @sobolev typedef signed char         int8;
			// @sobolev typedef short               int16;
			// TODO: Remove these type guards.  These are to avoid conflicts with
			// obsolete/protypes.h in the Gecko SDK.
			#ifndef _INT32
				#define _INT32
				typedef int                 int32;
			#endif
			// The NSPR system headers define 64-bit as |long| when possible.  In order to
			// not have typedef mismatches, we do the same on LP64.
			#if __LP64__
				// @sobolev typedef long      int64;
			#else
				// @sobolev typedef long long int64;
			#endif
			// NOTE: unsigned types are DANGEROUS in loops and other arithmetical
			// places.  Use the signed types unless your variable represents a bit
			// pattern (eg a hash value) or you really need the extra bit.  Do NOT
			// use 'unsigned' to express "this value should always be positive";
			// use assertions for this.
			// @sobolev typedef unsigned char      uint8;
			// @sobolev typedef unsigned short     uint16;
			// TODO: Remove these type guards.  These are to avoid conflicts with
			// obsolete/protypes.h in the Gecko SDK.
			#ifndef _UINT32
				#define _UINT32
				typedef unsigned int       uint32;
			#endif
			// See the comment above about NSPR and 64-bit.
			#if __LP64__
				// @sobolev typedef unsigned long uint64;
			#else
				// @sobolev typedef unsigned long long uint64;
			#endif
		#endif // !INT64_MAX

		typedef signed char         schar;
		// A type to represent a Unicode code-point value. As of Unicode 4.0,
		// such values require up to 21 bits.
		// (For type-checking on pointers, make this explicitly signed,
		// and it should always be the signed version of whatever int32 is.)
		typedef signed int         char32;
		// A macro to disallow the copy constructor and operator= functions
		// This should be used in the private: declarations for a class
		#if !defined(DISALLOW_COPY_AND_ASSIGN)
			#define DISALLOW_COPY_AND_ASSIGN(TypeName) TypeName(const TypeName&); void operator=(const TypeName&)
		#endif
		// The arraysize(arr) macro returns the # of elements in an array arr.
		// The expression is a compile-time constant, and therefore can be
		// used in defining new arrays, for example.  If you use arraysize on
		// a pointer by mistake, you will get a compile-time error.
		//
		// One caveat is that arraysize() doesn't accept any array of an
		// anonymous type or a type defined inside a function.  In these rare
		// cases, you have to use the unsafe ARRAYSIZE_UNSAFE() macro below.  This is
		// due to a limitation in C++'s template system.  The limitation might
		// eventually be removed, but it hasn't happened yet.

		// This template function declaration is used in defining arraysize.
		// Note that the function doesn't need an implementation, as we only
		// use its type.
		template <typename T, size_t N> char (&ArraySizeHelper(T (&array)[N]))[N];
		// That gcc wants both of these prototypes seems mysterious. VC, for
		// its part, can't decide which to use (another mystery). Matching of
		// template overloads: the final frontier.
		#ifndef _MSC_VER
			template <typename T, size_t N> char (&ArraySizeHelper(const T (&array)[N]))[N];
		#endif
		#if !defined(arraysize)
			#define arraysize(array) (sizeof(ArraySizeHelper(array)))
		#endif
		// ARRAYSIZE_UNSAFE performs essentially the same calculation as arraysize,
		// but can be used on anonymous types or types defined inside
		// functions.  It's less safe than arraysize as it accepts some
		// (although not all) pointers.  Therefore, you should use arraysize
		// whenever possible.
		//
		// The expression ARRAYSIZE_UNSAFE(a) is a compile-time constant of type
		// size_t.
		//
		// ARRAYSIZE_UNSAFE catches a few type errors.  If you see a compiler error
		//
		//   "warning: division by zero in ..."
		//
		// when using ARRAYSIZE_UNSAFE, you are (wrongfully) giving it a pointer.
		// You should only use ARRAYSIZE_UNSAFE on statically allocated arrays.
		//
		// The following comments are on the implementation details, and can
		// be ignored by the users.
		//
		// ARRAYSIZE_UNSAFE(arr) works by inspecting sizeof(arr) (the # of bytes in
		// the array) and sizeof(*(arr)) (the # of bytes in one array
		// element).  If the former is divisible by the latter, perhaps arr is
		// indeed an array, in which case the division result is the # of
		// elements in the array.  Otherwise, arr cannot possibly be an array,
		// and we generate a compiler error to prevent the code from
		// compiling.
		//
		// Since the size of bool is implementation-defined, we need to cast
		// !(sizeof(a) & sizeof(*(a))) to size_t in order to ensure the final
		// result has type size_t.
		//
		// This macro is not perfect as it wrongfully accepts certain
		// pointers, namely where the pointer size is divisible by the pointee
		// size.  Since all our code has to go through a 32-bit compiler,
		// where a pointer is 4 bytes, this means all pointers to a type whose
		// size is 3 or greater than 4 will be (righteously) rejected.
		#if !defined(ARRAYSIZE_UNSAFE)
			#define ARRAYSIZE_UNSAFE(a) ((sizeof(a) / sizeof(*(a))) / static_cast<size_t>(!(sizeof(a) % sizeof(*(a)))))
		#endif
		// The COMPILE_ASSERT macro can be used to verify that a compile time
		// expression is true. For example, you could use it to verify the
		// size of a static array:
		//
		//   COMPILE_ASSERT(ARRAYSIZE_UNSAFE(content_type_names) == CONTENT_NUM_TYPES,
		//                  content_type_names_incorrect_size);
		//
		// or to make sure a struct is smaller than a certain size:
		//
		//   COMPILE_ASSERT(sizeof(foo) < 128, foo_too_large);
		//
		// The second argument to the macro is the name of the variable. If
		// the expression is false, most compilers will issue a warning/error
		// containing the name of the variable.
		#if __cplusplus >= 201103L
			// Under C++11, just use static_assert.
			#define COMPILE_ASSERT(expr, msg) static_assert(expr, #msg)
		#else
			template <bool> struct CompileAssert {};
			// Annotate a variable indicating it's ok if the variable is not used.
			// (Typically used to silence a compiler warning when the assignment
			// is important for some other reason.)
			// Use like:
			//   int x ALLOW_UNUSED = ...;
			#if defined(__GNUC__)
				#define ALLOW_UNUSED __attribute__((unused))
			#else
				#define ALLOW_UNUSED
			#endif
			#define COMPILE_ASSERT(expr, msg) typedef CompileAssert<(bool(expr))> msg[bool(expr) ? 1 : -1] ALLOW_UNUSED
			// Implementation details of COMPILE_ASSERT:
			//
			// - COMPILE_ASSERT works by defining an array type that has -1
			//   elements (and thus is invalid) when the expression is false.
			//
			// - The simpler definition
			//
			//     #define COMPILE_ASSERT(expr, msg) typedef char msg[(expr) ? 1 : -1]
			//
			//   does not work, as gcc supports variable-length arrays whose sizes
			//   are determined at run-time (this is gcc's extension and not part
			//   of the C++ standard).  As a result, gcc fails to reject the
			//   following code with the simple definition:
			//
			//     int foo;
			//     COMPILE_ASSERT(foo, msg); // not supposed to compile as foo is
			//                               // not a compile-time constant.
			//
			// - By using the type CompileAssert<(bool(expr))>, we ensures that
			//   expr is a compile-time constant.  (Template arguments must be
			//   determined at compile-time.)
			//
			// - The outer parentheses in CompileAssert<(bool(expr))> are necessary
			//   to work around a bug in gcc 3.4.4 and 4.0.1.  If we had written
			//
			//     CompileAssert<bool(expr)>
			//
			//   instead, these compilers will refuse to compile
			//
			//     COMPILE_ASSERT(5 > 0, some_message);
			//
			//   (They seem to think the ">" in "5 > 0" marks the end of the
			//   template argument list.)
			//
			// - The array size is (bool(expr) ? 1 : -1), instead of simply
			//
			//     ((expr) ? 1 : -1).
			//
			//   This is to avoid running into a bug in MS VC 7.1, which
			//   causes ((0.0) ? 1 : -1) to incorrectly evaluate to 1.
		#endif
	}  // namespace phonenumbers
}  // namespace i18n

#endif  // I18N_PHONENUMBERS_BASE_BASICTYPES_H_
