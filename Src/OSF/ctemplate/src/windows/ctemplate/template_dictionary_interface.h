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

// ---
// Author: williasr@google.com (Scott Williams)
//
// This file implements the TemplateDictionaryInterface class. This interface
// forms the root of the TemplateDictionary class tree, but the interface is
// minimal enough to allow other sources of template data. Note that the
// TemplateDictionaryInterface class enumerates the properties expected by
// Template: it doesn't constrain how data gets into the
// TemplateDictionaryInterface class to begin with. For these methods, see
// TemplateDictionary.
//

#ifndef TEMPLATE_TEMPLATE_DICTIONARY_INTERFACE_H_
#define TEMPLATE_TEMPLATE_DICTIONARY_INTERFACE_H_

#include <stdlib.h>
#include <string>
#include <ctemplate/template_string.h>

// NOTE: if you are statically linking the template library into your binary
// (rather than using the template .dll), set '/D CTEMPLATE_DLL_DECL='
// as a compiler flag in your project file to turn off the dllimports.
#ifndef CTEMPLATE_DLL_DECL
	#define CTEMPLATE_DLL_DECL  // @sobolev __declspec(dllimport)
#endif

namespace ctemplate {

const int kIndent = 2;  // num spaces to indent each level -- used with dump

// TemplateDictionaryInterface
// The template data contains the associated values for
// variables, the hidden/visible state for sections and included
// templates, the associated set of dictionaries for sections and
// included templates, and the template filenames to be expanded in
// place of template-include nodes.
class CTEMPLATE_DLL_DECL TemplateDictionaryInterface {
 public:
  // TemplateDictionaryInterface destructor
  virtual ~TemplateDictionaryInterface() {}

 protected:
  // The interface as follows is used at expand-time by Expand.
  friend class VariableTemplateNode;
  friend class SectionTemplateNode;
  friend class TemplateTemplateNode;
  // This class reaches into our internals for testing.
  friend class TemplateDictionaryPeer;
  friend class TemplateDictionaryPeerIterator;

  // GetSectionValue
  //   Returns the value of a variable.
  virtual TemplateString GetValue(const TemplateString& variable) const = 0;

  // IsHiddenSection
  //   A predicate to indicate the current hidden/visible state of a section
  //   whose name is passed to it.
  virtual bool IsHiddenSection(const TemplateString& name) const = 0;

  // Dump a string representation of this dictionary to the supplied string.
  virtual void DumpToString(std::string* out, int level) const = 0;

  // TemplateDictionaryInterface is an abstract class, so its constructor is
  // only visible to its subclasses.
  TemplateDictionaryInterface() {}

  class Iterator {
   protected:
    Iterator() { }
   public:
    virtual ~Iterator() { }

    // Returns false if the iterator is exhausted.
    virtual bool HasNext() const = 0;

    // Returns the current referent and increments the iterator to the next.
    virtual const TemplateDictionaryInterface& Next() = 0;
  };

  // IsHiddenTemplate
  //   Returns true if the template include is hidden. This is analogous to
  //   IsHiddenSection, but for template nodes.
  virtual bool IsHiddenTemplate(const TemplateString& name) const = 0;

  // GetIncludeTemplateName
  //   Returns the name of the template associated with the given template
  //   include variable. If more than one dictionary is attached to the include
  //   symbol, dictnum can be used to disambiguate which include name you mean.
  virtual const char* GetIncludeTemplateName(
      const TemplateString& variable, int dictnum) const = 0;

  // CreateTemplateIterator
  //   A factory method for constructing an iterator representing the
  //   subdictionaries of the given include node.  The caller is
  //   responsible for deleting the return value when it's done with it.
  virtual Iterator* CreateTemplateIterator(
      const TemplateString& section) const = 0;

  // CreateSectionIterator
  //   A factory method for constructing an iterator representing the
  //   subdictionaries of the given section node.  The caller is
  //   responsible for deleting the return value when it's done with it.
  virtual Iterator* CreateSectionIterator(
      const TemplateString& section) const = 0;

  // IsUnhiddenSection
  //   Returns true if the section has been marked visible and false otherwise.
  virtual bool IsUnhiddenSection(
      const TemplateString& name) const = 0;

 private:
  // Disallow copy and assign.
  TemplateDictionaryInterface(const TemplateDictionaryInterface&);
  void operator=(const TemplateDictionaryInterface&);
};

}

#endif  // TEMPLATE_TEMPLATE_DICTIONARY_INTERFACE_H_
