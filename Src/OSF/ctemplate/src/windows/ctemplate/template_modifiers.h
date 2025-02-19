// Copyright (c) 2007, Google Inc.
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
// We allow template variables to have modifiers, each possibly with a
// value associated with it.  Format is
//    {{VARNAME:modname[=modifier-value]:modname[=modifier-value]:...}}
// Modname refers to a functor that takes the variable's value
// and modifier-value (empty-string if no modifier-value was
// specified), and returns a munged value.  Modifiers are applied
// left-to-right.  We define the legal modnames here, and the
// functors they refer to.
//
// Modifiers have a long-name, an optional short-name (one char;
// may be \0 if you don't want a shortname), and a functor that's
// applied to the variable.
//
// In addition to the list of modifiers hard-coded in the source code
// here, it is possible to dynamicly register modifiers using a long
// name starting with "x-".  If you wish to define your own modifier
// class, in your own source code, just subclass TemplateModifier --
// see template_modifiers.cc for details of how to do that.
//
// Adding a new built-in modifier, to this file, takes several steps,
// both in this .h file and in the corresponding .cc file:
// 1) .h file: Define a struct for the modifier.  It must subclass
//     TemplateModifier.
// 2) .h file: declare a variable that's an instance of the struct.
//    This is used for people who want to modify the string themselves,
//    via TemplateDictionary::SetEscapedValue.
// 5) .cc file: define the new modifier's Modify method.
// 6) .cc file: give storage for the variable declared in the .h file (in 2).
// 7) .cc file: add the modifier to the g_modifiers array.

#ifndef TEMPLATE_TEMPLATE_MODIFIERS_H_
#define TEMPLATE_TEMPLATE_MODIFIERS_H_

#include <sys/types.h>   // for size_t
#include <string>
#include <ctemplate/template_emitter.h>   // so we can inline operator()
#include <ctemplate/per_expand_data.h>    // could probably just forward-declare

// NOTE: if you are statically linking the template library into your binary
// (rather than using the template .dll), set '/D CTEMPLATE_DLL_DECL='
// as a compiler flag in your project file to turn off the dllimports.
#ifndef CTEMPLATE_DLL_DECL
	#define CTEMPLATE_DLL_DECL  // @sobolev __declspec(dllimport)
#endif

namespace ctemplate {
class Template;

#define MODIFY_SIGNATURE_                                               \
public:                                                                \
	virtual void Modify(const char* in, size_t inlen,                     \
	    const PerExpandData*, ExpandEmitter* outbuf,      \
	    const std::string& arg) const

// If you wish to write your own modifier, it should subclass this
// method.  Your subclass should only define Modify(); for efficiency,
// we do not make operator() virtual.
class CTEMPLATE_DLL_DECL TemplateModifier {
public:
	// This function takes a string as input, a char*/size_t pair, and
	// appends the modified version to the end of outbuf.  In addition
	// to the variable-value to modify (specified via in/inlen), each
	// Modify passes in two pieces of user-supplied data:
	// 1) arg: this is the modifier-value, for modifiers that take a
	//         value (e.g. "{{VAR:modifier=value}}").  This value
	//         comes from the template file.  For modifiers that take
	//         no modval argument, arg will always be "".  For modifiers
	//         that do take such an argument, arg will always start with "=".
	// 2) per_expand_data: this is a set of data that the application can
	//         associate with a TemplateDictionary, and is passed in to
	//         every variable expanded using that dictionary.  This value
	//         comes from the source code.
	virtual void Modify(const char* in, size_t inlen,
	    const PerExpandData* per_expand_data,
	    ExpandEmitter* outbuf,
	    const std::string& arg) const = 0;

	// This function can be used to speed up modification.  If Modify()
	// is often a noop, you can implement MightModify() to indicate
	// situations where it's safe to avoid the call to Modify(), because
	// Modify() won't do any modifications in this case.  Note it's
	// always safe to return true here; you should just return false if
	// you're certain Modify() can be ignored.  This function is
	// advisory; the template system is not required to call
	// MightModify() before Modify().
	virtual bool MightModify(const PerExpandData* /*per_expand_data*/,
	    const std::string& /*arg*/) const {
		return true;
	}

	// We support both modifiers that take an argument, and those that don't.
	// We also support passing in a string, or a char*/int pair.
	std::string operator()(const char* in, size_t inlen, const std::string& arg = "") const {
		std::string out;
		// we'll reserve some space to account for minimal escaping: say 12%
		out.reserve(inlen + inlen/8 + 16);
		StringEmitter outbuf(&out);
		Modify(in, inlen, NULL, &outbuf, arg);
		return out;
	}

	std::string operator()(const std::string& in, const std::string& arg = "") const {
		return operator()(in.data(), in.size(), arg);
	}

	virtual ~TemplateModifier(); // always need a virtual destructor!
};

// Returns the input verbatim (for testing)
class CTEMPLATE_DLL_DECL NullModifier : public TemplateModifier {
	MODIFY_SIGNATURE_;
};

extern CTEMPLATE_DLL_DECL NullModifier null_modifier;

// Escapes < > " ' & <non-space whitespace> to &lt; &gt; &quot;
// &#39; &amp; <space>
class CTEMPLATE_DLL_DECL HtmlEscape : public TemplateModifier {
	MODIFY_SIGNATURE_;
};

extern CTEMPLATE_DLL_DECL HtmlEscape html_escape;

// Same as HtmlEscape but leaves all whitespace alone. Eg. for <pre>..</pre>
class CTEMPLATE_DLL_DECL PreEscape : public TemplateModifier {
	MODIFY_SIGNATURE_;
};

extern CTEMPLATE_DLL_DECL PreEscape pre_escape;

// Like HtmlEscape but allows HTML entities, <br> tags, <wbr> tags,
// matched <b> and </b> tags, matched <i> and </i> tags, matched <em> and </em>
// tags, and matched <span dir=(rtl|ltr)> tags.
class CTEMPLATE_DLL_DECL SnippetEscape : public TemplateModifier {
	MODIFY_SIGNATURE_;
};

extern CTEMPLATE_DLL_DECL SnippetEscape snippet_escape;

// Replaces characters not safe for an unquoted attribute with underscore.
// Safe characters are alphanumeric, underscore, dash, period, and colon.
// The equal sign is also considered safe unless it is at the start
// or end of the input in which case it is replaced with underscore.
//
// We added the equal sign to the safe characters to allow this modifier
// to be used on attribute name/value pairs in HTML tags such as
//   <div {{CLASS:H=attribute}}>
// where CLASS is expanded to "class=bla".
//
// Note: The equal sign is replaced when found at either boundaries of the
// string due to the concern it may be lead to XSS under some special
// circumstances: Say, if this string is the value of an attribute in an
// HTML tag and ends with an equal sign, a browser may possibly end up
// interpreting the next token as the value of this string rather than
// a new attribute (esoteric).
class CTEMPLATE_DLL_DECL CleanseAttribute : public TemplateModifier {
	MODIFY_SIGNATURE_;
};

extern CTEMPLATE_DLL_DECL CleanseAttribute cleanse_attribute;

// Removes characters not safe for a CSS value. Safe characters are
// alphanumeric, space, underscore, period, coma, exclamation mark,
// pound, percent, and dash.
class CTEMPLATE_DLL_DECL CleanseCss : public TemplateModifier {
	MODIFY_SIGNATURE_;
};

extern CTEMPLATE_DLL_DECL CleanseCss cleanse_css;

// Checks that a url is either an absolute http(s) URL or a relative
// url that doesn't have a protocol hidden in it (ie [foo.html] is
// fine, but not [javascript:foo]) and then performs another type of
// escaping. Returns the url escaped with the specified modifier if
// good, otherwise returns a safe replacement URL.
// This is normally "#", but for <img> tags, it is not safe to set
// the src attribute to "#".  This is because this causes some browsers
// to reload the page, which can cause a DoS.
class CTEMPLATE_DLL_DECL ValidateUrl : public TemplateModifier {
public:
	explicit ValidateUrl(const TemplateModifier& chained_modifier,
	    const char* unsafe_url_replacement)
		: chained_modifier_(chained_modifier),
		unsafe_url_replacement_(unsafe_url_replacement),
		unsafe_url_replacement_length_(strlen(unsafe_url_replacement)) {
	}

	MODIFY_SIGNATURE_;
	static const char* const kUnsafeUrlReplacement;
	static const char* const kUnsafeImgSrcUrlReplacement;
private:
	const TemplateModifier& chained_modifier_;
	const char* unsafe_url_replacement_;
	int unsafe_url_replacement_length_;
};

extern CTEMPLATE_DLL_DECL ValidateUrl validate_url_and_html_escape;
extern CTEMPLATE_DLL_DECL ValidateUrl validate_url_and_javascript_escape;
extern CTEMPLATE_DLL_DECL ValidateUrl validate_url_and_css_escape;
extern CTEMPLATE_DLL_DECL ValidateUrl validate_img_src_url_and_html_escape;
extern CTEMPLATE_DLL_DECL ValidateUrl validate_img_src_url_and_javascript_escape;
extern CTEMPLATE_DLL_DECL ValidateUrl validate_img_src_url_and_css_escape;

// Escapes < > & " ' to &lt; &gt; &amp; &quot; &#39; (same as in HtmlEscape).
// If you use it within a CDATA section, you may be escaping more characters
// than strictly necessary. If this turns out to be an issue, we will need
// to add a variant just for CDATA.
class CTEMPLATE_DLL_DECL XmlEscape : public TemplateModifier {
	MODIFY_SIGNATURE_;
};

extern CTEMPLATE_DLL_DECL XmlEscape xml_escape;

// Escapes characters that cannot appear unescaped in a javascript string
// assuming UTF-8 encoded input.
// This does NOT escape all characters that cannot appear unescaped in a
// javascript regular expression literal.
class CTEMPLATE_DLL_DECL JavascriptEscape : public TemplateModifier {
	MODIFY_SIGNATURE_;
};

extern CTEMPLATE_DLL_DECL JavascriptEscape javascript_escape;

// Checks that the input is a valid javascript non-string literal
// meaning a boolean (true, false) or a numeric value (decimal, hex or octal).
// If valid, we output the input as is, otherwise we output null instead.
// Input of zero length is considered valid and nothing is output.
//
// The emphasis is on safety against injection of javascript code rather
// than perfect validation, as such it is possible for non-valid literals to
// pass through.
//
// You would use this modifier for javascript variables that are not
// enclosed in quotes such as:
//    <script>var a = {{VALUE}};</script> OR
//    <a href="url" onclick="doSubmit({{ID}})">
// For variables that are quoted (i.e. string literals) use javascript_escape.
//
// Limitations:
// . NaN, +/-Infinity and null are not recognized.
// . Output is not guaranteed to be a valid literal,
//   e.g: +55+-e34 will output as is.
//   e.g: trueeee will output nothing as it is not a valid boolean.
//
// Details:
// . For Hex numbers, it checks for case-insensitive 0x[0-9A-F]+
//   that should be a proper check.
// . For other numbers, it checks for case-insensitive [0-9eE+-.]*
//   so can also accept invalid numbers such as the number 5..45--10.
// . "true" and "false" (without quotes) are also accepted and that's it.
//
class CTEMPLATE_DLL_DECL JavascriptNumber : public TemplateModifier {
	MODIFY_SIGNATURE_;
};

extern CTEMPLATE_DLL_DECL JavascriptNumber javascript_number;

// Escapes characters not in [0-9a-zA-Z.,_:*/~!()-] as %-prefixed hex.
// Space is encoded as a +.
class CTEMPLATE_DLL_DECL UrlQueryEscape : public TemplateModifier {
	MODIFY_SIGNATURE_;
};

extern CTEMPLATE_DLL_DECL UrlQueryEscape url_query_escape;

// Escapes " \ / <FF> <CR> <LF> <BS> <TAB> to \" \\ \/ \f \r \n \b \t
// Also escapes < > & to their corresponding \uXXXX representation
// (\u003C, \u003E, \u0026 respectively).
class CTEMPLATE_DLL_DECL JsonEscape : public TemplateModifier {
	MODIFY_SIGNATURE_;
};

extern CTEMPLATE_DLL_DECL JsonEscape json_escape;

// Inserts the given prefix (given as the argument to this modifier)
// after every newline in the text.  Note that it does *not* insert
// prefix at the very beginning of the text -- in its expected use,
// that prefix will already be present before this text, in the
// template.  This is meant to be used internally, and is not exported
// via the g_modifiers list.
class CTEMPLATE_DLL_DECL PrefixLine : public TemplateModifier {
	MODIFY_SIGNATURE_;
};

extern CTEMPLATE_DLL_DECL PrefixLine prefix_line;

#undef MODIFY_SIGNATURE_

// Registers a new template modifier.
// long_name must start with "x-".
// If the modifier takes a value (eg "{{VAR:x-name=value}}"), then
// long_name should end with "=".  This is similar to getopt(3) syntax.
// We also allow value-specializations, with specific values specified
// as part of long-name.  For instance:
//    AddModifier("x-mod=", &my_modifierA);
//    AddModifier("x-mod=bar", &my_modifierB);
//    AddModifier("x-mod2", &my_modifierC);
// For the template
//    {{VAR1:x-mod=foo}} {{VAR2:x-mod=bar}} {{VAR3:x-mod=baz}} {{VAR4:x-mod2}}
// VAR1 and VAR3 would get modified by my_modifierA, VAR2 by my_modifierB,
// and VAR4 by my_modifierC.  The order of the AddModifier calls is not
// significant.
extern CTEMPLATE_DLL_DECL bool AddModifier(const char* long_name, const TemplateModifier* modifier);

// Same as AddModifier() above except that the modifier is considered
// to produce safe output that can be inserted in any context without
// the need for additional escaping. This difference only impacts
// the Auto-Escape mode: In that mode, when a variable (or template-include)
// has a modifier added via AddXssSafeModifier(), it is excluded from
// further escaping, effectively treated as though it had the :none modifier.
// Because Auto-Escape is disabled for any variable and template-include
// that includes such a modifier, use this function with care and ensure
// that it may not emit harmful output that could lead to XSS.
//
// Some valid uses of AddXssSafeModifier:
// . A modifier that converts a string to an integer since
//   an integer is generally safe in any context.
// . A modifier that returns one of a fixed number of safe values
//   depending on properties of the input.
//
// Some not recommended uses of AddXssSafeModifier:
// . A modifier that applies some extra formatting to the input
//   before returning it since the output will still contain
//   harmful content if the input does.
// . A modifier that applies one type of escaping to the input
//   (say HTML-escape). This may be dangerous when the modifier
//   is used in a different context (say Javascript) where this
//   escaping may be inadequate.
extern CTEMPLATE_DLL_DECL bool AddXssSafeModifier(const char* long_name, const TemplateModifier* modifier);
}

#endif  // TEMPLATE_TEMPLATE_MODIFIERS_H_
