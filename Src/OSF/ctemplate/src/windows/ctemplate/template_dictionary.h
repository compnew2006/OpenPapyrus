// Copyright (c) 2006, Google Inc.
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

// ---
// Author: csilvers@google.com (Craig Silverstein)
//
// Based on the 'old' TemplateDictionary by Frank Jernigan.
//
// A template dictionary maps names (as found in template files)
// to their values.  There are three types of names:
//   variables: value is a string.
//   sections: value is a list of sub-dicts to use when expanding the section;
//             the section is expanded once per sub-dict.
//   template-include: value is a list of pairs: name of the template file
//             to include, and the sub-dict to use when expanding it.
// TemplateDictionary has routines for setting these values.
//
// For (many) more details, see the doc/ directory.

#ifndef TEMPLATE_TEMPLATE_DICTIONARY_H_
#define TEMPLATE_TEMPLATE_DICTIONARY_H_

#include <stdarg.h>      // for StringAppendV()
#include <stddef.h>      // for size_t and ptrdiff_t
#include <stdlib.h>      // for NULL
#include <sys/types.h>
#include <functional>    // for less<>
#include <map>
#include <string>
#include <vector>

#include <ctemplate/str_ref.h>
#include <ctemplate/template_dictionary_interface.h>
#include <ctemplate/template_modifiers.h>
#include <ctemplate/template_string.h>

// NOTE: if you are statically linking the template library into your binary
// (rather than using the template .dll), set '/D CTEMPLATE_DLL_DECL='
// as a compiler flag in your project file to turn off the dllimports.
#ifndef CTEMPLATE_DLL_DECL
	#define CTEMPLATE_DLL_DECL  // @sobolev __declspec(dllimport)
#endif

namespace ctemplate {
template <class T, class C> class ArenaAllocator;
class UnsafeArena;
template <typename A, int B, typename C, typename D> class small_map;
template <typename NormalMap> class small_map_default_init;  // in small_map.h

class CTEMPLATE_DLL_DECL TemplateDictionary : public TemplateDictionaryInterface {
public:
	// name is used only for debugging.
	// arena is used to store all names and values.  It can be NULL (the
	//    default), in which case we create own own arena.
	explicit TemplateDictionary(const TemplateString& name,
	    UnsafeArena* arena = NULL);
	~TemplateDictionary();

	// If you want to be explicit, you can use NO_ARENA as a synonym to NULL.
	static UnsafeArena* const NO_ARENA;

	std::string name() const {
		return std::string(name_.data(), name_.size());
	}

	// Returns a recursive copy of this dictionary.  This dictionary
	// *must* be a "top-level" dictionary (that is, not created via
	// AddSectionDictionary() or AddIncludeDictionary()).  Caller owns
	// the resulting dict, and must delete it.  If arena is NULL, we
	// create our own.  Returns NULL if the copy fails (probably because
	// the "top-level" rule was violated).
	TemplateDictionary* MakeCopy(const TemplateString& name_of_copy,
	    UnsafeArena* arena = NULL);

	// --- Routines for VARIABLES
	// These are the five main routines used to set the value of a variable.
	// As always, wherever you see TemplateString, you can also pass in
	// either a char* or a C++ string, or a TemplateString(s, slen).

	void SetValue(const TemplateString variable, const TemplateString value);
	void SetIntValue(const TemplateString variable, long value);
	void SetFormattedValue(const TemplateString variable, const char* format, ...)
#if 0
	__attribute__((__format__(__printf__, 3, 4)))
#endif
	; // starts at 3 because of implicit 1st arg 'this'

	class SetProxy {
public:
		SetProxy(TemplateDictionary& dict, const TemplateString& variable) :
			dict_(dict),
			variable_(variable) {
		}

		void operator=(str_ref value) {
			dict_.SetValue(variable_, TemplateString(value.data(), value.size()));
		}

		void operator=(long value) {
			dict_.SetIntValue(variable_, value);
		}

private:
		TemplateDictionary& dict_;
		const TemplateString& variable_;
	};

	SetProxy operator[](const TemplateString& variable) {
		return SetProxy(*this, variable);
	}

	// We also let you set values in the 'global' dictionary which is
	// referenced when all other dictionaries fail.  Note this is a
	// static method: no TemplateDictionary instance needed.  Since
	// this routine is rarely used, we don't provide variants.
	static void SetGlobalValue(const TemplateString variable,
	    const TemplateString value);

	// This is used for a value that you want to be 'global', but only
	// in the scope of a given template, including all its sections and
	// all its sub-included dictionaries.  The main difference between
	// SetTemplateGlobalValue() and SetValue(), is that
	// SetTemplateGlobalValue() values persist across template-includes.
	// This is intended for session-global data; since that should be
	// fairly rare, we don't provide variants.
	void SetTemplateGlobalValue(const TemplateString variable,
	    const TemplateString value);

	// Similar SetTemplateGlobalValue above, this method shows a section in this
	// template, all its sections, and all its template-includes. This is intended
	// for session-global data, for example allowing you to show variant portions
	// of your template for certain browsers/languages without having to call
	// ShowSection on each template you use.
	void ShowTemplateGlobalSection(const TemplateString variable);

	// These routines are like SetValue and SetTemplateGlobalValue, but
	// they do not make a copy of the input data. THE CALLER IS
	// RESPONSIBLE FOR ENSURING THE PASSED-IN STRINGS LIVE FOR AT LEAST
	// AS LONG AS THIS DICTIONARY! In general, they yield a quite minor
	// performance increase for significant increased code fragility,
	// so do not use them unless you really need the speed improvements.
	void SetValueWithoutCopy(const TemplateString variable,
	    const TemplateString value);
	void SetTemplateGlobalValueWithoutCopy(const TemplateString variable,
	    const TemplateString value);

	// --- Routines for SECTIONS
	// We show a section once per dictionary that is added with its name.
	// Recall that lookups are hierarchical: if a section tried to look
	// up a variable in its sub-dictionary and fails, it will look next
	// in its parent dictionary (us).  So it's perfectly appropriate to
	// keep the sub-dictionary empty: that will show the section once,
	// and take all var definitions from us.  ShowSection() is a
	// convenience routine that does exactly that.

	// Creates an empty dictionary whose parent is us, and returns it.
	// As always, wherever you see TemplateString, you can also pass in
	// either a char* or a C++ string, or a TemplateString(s, slen).
	TemplateDictionary* AddSectionDictionary(const TemplateString section_name);
	void ShowSection(const TemplateString section_name);

	// A convenience method.  Often a single variable is surrounded by
	// some HTML that should not be printed if the variable has no
	// value.  The way to do this is to put that html in a section.
	// This method makes it so the section is shown exactly once, with a
	// dictionary that maps the variable to the proper value.  If the
	// value is "", on the other hand, this method does nothing, so the
	// section remains hidden.
	void SetValueAndShowSection(const TemplateString variable,
	    const TemplateString value,
	    const TemplateString section_name);

	// --- Routines for TEMPLATE-INCLUDES
	// Included templates are treated like sections, but they require
	// the name of the include-file to go along with each dictionary.

	TemplateDictionary* AddIncludeDictionary(const TemplateString variable);

	// This is required for include-templates; it specifies what template
	// to include.  But feel free to call this on any dictionary, to
	// document what template-file the dictionary is intended to go with.
	void SetFilename(const TemplateString filename);

	// --- DEBUGGING TOOLS

	// Logs the contents of a dictionary and its sub-dictionaries.
	// Dump goes to stdout/stderr, while DumpToString goes to the given string.
	// 'indent' is how much to indent each line of the output.
	void Dump(int indent = 0) const;
	virtual void DumpToString(std::string* out, int indent = 0) const;

	// --- DEPRECATED ESCAPING FUNCTIONALITY

	// Escaping in the binary has been deprecated in favor of using modifiers
	// to do the escaping in the template:
	//            "...{{MYVAR:html_escape}}..."
	void SetEscapedValue(const TemplateString variable, const TemplateString value,
	    const TemplateModifier& escfn);
	void SetEscapedFormattedValue(const TemplateString variable,
	    const TemplateModifier& escfn,
	    const char* format, ...)
#if 0
	__attribute__((__format__(__printf__, 4, 5)))
#endif
	; // starts at 4 because of implicit 1st arg 'this'

private:
	friend class SectionTemplateNode; // for access to GetSectionValue(), etc.
	friend class TemplateTemplateNode; // for access to GetSectionValue(), etc.
	friend class VariableTemplateNode; // for access to GetSectionValue(), etc.
	// For unittesting code using a TemplateDictionary.
	friend class TemplateDictionaryPeer;

	class DictionaryPrinter; // nested class
	friend class DictionaryPrinter;

	// We need this functor to tell small_map how to create a map<> when
	// it decides to do so: we want it to create that map on the arena.
	class map_arena_init;

	typedef std::vector<TemplateDictionary*,
		ArenaAllocator<TemplateDictionary*, UnsafeArena> >
	    DictVector;
	// The '4' here is the size where small_map switches from vector<> to map<>.
	typedef small_map<std::map<TemplateId, TemplateString, std::less<TemplateId>,
		ArenaAllocator<std::pair<const TemplateId, TemplateString>,
		UnsafeArena> >,
		4, std::equal_to<TemplateId>, map_arena_init>
	    VariableDict;
	typedef small_map<std::map<TemplateId, DictVector*, std::less<TemplateId>,
		ArenaAllocator<std::pair<const TemplateId, DictVector*>,
		UnsafeArena> >,
		4, std::equal_to<TemplateId>, map_arena_init>
	    SectionDict;
	typedef small_map<std::map<TemplateId, DictVector*, std::less<TemplateId>,
		ArenaAllocator<std::pair<const TemplateId, DictVector*>,
		UnsafeArena> >,
		4, std::equal_to<TemplateId>, map_arena_init>
	    IncludeDict;
	// This is used only for global_dict_, which is just like a VariableDict
	// but does not bother with an arena (since this memory lives forever).
	typedef small_map<std::map<TemplateId, TemplateString, std::less<TemplateId> >,
		4, std::equal_to<TemplateId>,
		small_map_default_init<
			std::map<TemplateId, TemplateString,
			std::less<TemplateId> > > >
	    GlobalDict;

	// These are helper functions to allocate the parts of the dictionary
	// on the arena.
	template <typename T> inline void LazilyCreateDict(T** dict);
	inline void LazyCreateTemplateGlobalDict();
	inline DictVector* CreateDictVector();
	inline TemplateDictionary* CreateTemplateSubdict(const TemplateString& name,
	    UnsafeArena* arena,
	    TemplateDictionary* parent_dict,
	    TemplateDictionary* template_global_dict_owner);

	// This is a helper function to insert <key,value> into m.
	// Normally, we'd just use m[key] = value, but map rules
	// require default constructor to be public for that to compile, and
	// for some types we'd rather not allow that.  HashInsert also inserts
	// the key into an id(key)->key map, to allow for id-lookups later.
	template <typename MapType, typename ValueType>
	static void HashInsert(MapType* m, TemplateString key, ValueType value);

	// Constructor created for all children dictionaries. This includes
	// both a pointer to the parent dictionary and also the the
	// template-global dictionary from which all children (both
	// IncludeDictionary and SectionDictionary) inherit.  Values are
	// filled into global_template_dict via SetTemplateGlobalValue.
	explicit TemplateDictionary(const TemplateString& name,
	    class UnsafeArena* arena,
		    TemplateDictionary* parent_dict,
		    TemplateDictionary* template_global_dict_owner);

	// Helps set up the static stuff. Must be called exactly once before
	// accessing global_dict_.  GoogleOnceInit() is used to manage that
	// initialization in a thread-safe way.
	static void SetupGlobalDict();

	// Utility functions for copying a string into the arena.
	// Memdup also copies in a trailing NUL, which is why we have the
	// trailing-NUL check in the TemplateString version of Memdup.
	TemplateString Memdup(const char* s, size_t slen);
	TemplateString Memdup(const TemplateString& s) {
		if(s.is_immutable() && s.data()[s.size()] == '\0') {
			return s;
		}
		return Memdup(s.data(), s.size());
	}

	// Used for recursive MakeCopy calls.
	TemplateDictionary* InternalMakeCopy(const TemplateString& name_of_copy,
	    UnsafeArena* arena,
	    TemplateDictionary* parent_dict,
	    TemplateDictionary* template_global_dict_owner);

	// A helper for creating section and include dicts.
	static std::string CreateSubdictName(const TemplateString& dict_name, const TemplateString& sub_name,
	    size_t index, const char* suffix);

	// Must be called whenever we add a value to one of the dictionaries above,
	// to ensure that we can reconstruct the id -> string mapping.
	static void AddToIdToNameMap(TemplateId id, const TemplateString& str);

	// Used to do the formatting for the SetFormatted*() functions
	static int StringAppendV(char* space, char** out,
	    const char* format, va_list ap);

	// How Template::Expand() and its children access the template-dictionary.
	// These fill the API required by TemplateDictionaryInterface.
	virtual TemplateString GetValue(const TemplateString& variable) const;
	virtual bool IsHiddenSection(const TemplateString& name) const;
	virtual bool IsUnhiddenSection(const TemplateString& name) const {
		return !IsHiddenSection(name);
	}

	virtual bool IsHiddenTemplate(const TemplateString& name) const;
	virtual const char* GetIncludeTemplateName(const TemplateString& variable, int dictnum) const;

	// Determine whether there's anything set in this dictionary
	bool Empty() const;

	// This is needed by DictionaryPrinter because it's not a friend
	// of TemplateString, but we are
	static std::string PrintableTemplateString(const TemplateString& ts) {
		return std::string(ts.data(), ts.size());
	}

	static bool InvalidTemplateString(const TemplateString& ts) {
		return ts.data() == NULL;
	}

	// Compilers differ about whether nested classes inherit our friendship.
	// The only thing DictionaryPrinter needs is IdToString, so just re-export.
	static TemplateString IdToString(TemplateId id) { // for DictionaryPrinter
		return TemplateString::IdToString(id);
	}

	// CreateTemplateIterator
	//   This is SectionIterator exactly, just with a different name to
	//   self-document the fact the value applies to a template include.
	// Caller frees return value.
	virtual TemplateDictionaryInterface::Iterator* CreateTemplateIterator(const TemplateString& section_name) const;

	// CreateSectionIterator
	//   Factory method implementation that constructs a iterator representing the
	//   set of dictionaries associated with a section name, if any. This
	//   implementation checks the local dictionary itself, not the template-wide
	//   dictionary or the global dictionary.
	// Caller frees return value.
	virtual TemplateDictionaryInterface::Iterator* CreateSectionIterator(const TemplateString& section_name) const;

	// TemplateDictionary-specific implementation of dictionary iterators.
	template <typename T> // T is *TemplateDictionary::const_iterator
	class Iterator : public TemplateDictionaryInterface::Iterator {
protected:
		friend class TemplateDictionary;
		Iterator(T begin, T end) : begin_(begin), end_(end) {
		}

public:
		virtual ~Iterator() {
		}

		virtual bool HasNext() const;
		virtual const TemplateDictionaryInterface& Next();
private:
		T begin_;
		const T end_;
	};

	// A small helper factory function for Iterator
	template <typename T>
	static Iterator<typename T::const_iterator>* MakeIterator(const T& dv) {
		return new Iterator<typename T::const_iterator>(dv.begin(), dv.end());
	}

	// The "name" of the dictionary for debugging output (Dump, etc.)
	// The arena, also set at construction time.
	class UnsafeArena* const arena_;
	bool should_delete_arena_; // only true if we 'new arena' in constructor
	TemplateString name_;  // points into the arena, or to static memory

	// The three dictionaries that I own -- for vars, sections, and template-incs
	VariableDict* variable_dict_;
	SectionDict* section_dict_;
	IncludeDict* include_dict_;

	// The template_global_dict is consulted if a lookup in the variable, section,
	// or include dicts named above fails. It forms a convenient place to store
	// session-specific data that's applicable to all templates in the dictionary
	// tree.
	// For the parent-template, template_global_dict_ is not NULL, and
	// template_global_dict_owner_ is this.  For all of its children,
	// template_global_dict_ is NULL, and template_global_dict_owner_ points to
	// the root parent-template (the one with the non-NULL template_global_dict_).
	TemplateDictionary* template_global_dict_;
	TemplateDictionary* template_global_dict_owner_;

	// My parent dictionary, used when variable lookups at this level fail.
	// Note this is only for *variables* and *sections*, not templates.
	TemplateDictionary* parent_dict_;
	// The static, global dictionary, at the top of the parent-dictionary chain
	static GlobalDict* global_dict_;
	static TemplateString* empty_string_; // what is returned on lookup misses

	// The filename associated with this dictionary.  If set, this declares
	// what template the dictionary is supposed to be expanded with.  Required
	// for template-includes, optional (but useful) for 'normal' dicts.
	const char* filename_;

private:
	// Can't invoke copy constructor or assignment operator
	TemplateDictionary(const TemplateDictionary&);
	void operator=(const TemplateDictionary&);
};
}

#endif  // TEMPLATE_TEMPLATE_DICTIONARY_H_
