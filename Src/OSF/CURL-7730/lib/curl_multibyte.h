#ifndef HEADER_CURL_MULTIBYTE_H
#define HEADER_CURL_MULTIBYTE_H
/***************************************************************************
 *                            _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                       / __| | | | |_) | |
 *                      | (__| |_| |  _ <| |___
 *                       \___|\___/|_| \_\_____|
 *
 * Copyright (C) 1998 - 2020, Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at https://curl.haxx.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ***************************************************************************/
#include "curl_setup.h"

#if defined(WIN32)

 /*
  * MultiByte conversions using Windows kernel32 library.
  */

wchar_t *curlx_convert_UTF8_to_wchar(const char *str_utf8);
char *curlx_convert_wchar_to_UTF8(const wchar_t *str_w);

#endif /* WIN32 */

/*
 * Macros curlx_convert_UTF8_to_tchar(), curlx_convert_tchar_to_UTF8()
 * and curlx_unicodefree() main purpose is to minimize the number of
 * preprocessor conditional directives needed by code using these
 * to differentiate UNICODE from non-UNICODE builds.
 *
 * When building with UNICODE defined, these two macros
 * curlx_convert_UTF8_to_tchar() and curlx_convert_tchar_to_UTF8()
 * return a pointer to a newly allocated memory area holding result.
 * When the result is no longer needed, allocated memory is intended
 * to be free'ed with curlx_unicodefree().
 *
 * When building without UNICODE defined, this macros
 * curlx_convert_UTF8_to_tchar() and curlx_convert_tchar_to_UTF8()
 * return the pointer received as argument. curlx_unicodefree() does
 * no actual free'ing of this pointer it is simply set to NULL.
 */

#if defined(UNICODE) && defined(WIN32)

#define curlx_convert_UTF8_to_tchar(ptr) curlx_convert_UTF8_to_wchar((ptr))
#define curlx_convert_tchar_to_UTF8(ptr) curlx_convert_wchar_to_UTF8((ptr))
#define curlx_unicodefree(ptr)                          \
  do {                                                  \
    if(ptr) {                                           \
      (free)(ptr);                                        \
      (ptr) = NULL;                                     \
    }                                                   \
  } while(0)

typedef union {
  ushort       *tchar_ptr;
  const ushort *const_tchar_ptr;
  ushort       *tbyte_ptr;
  const ushort *const_tbyte_ptr;
} xcharp_u;

#else

#define curlx_convert_UTF8_to_tchar(ptr) (ptr)
#define curlx_convert_tchar_to_UTF8(ptr) (ptr)
#define curlx_unicodefree(ptr) \
  do {(ptr) = NULL;} while(0)

typedef union {
  char                *tchar_ptr;
  const char          *const_tchar_ptr;
  uchar       *tbyte_ptr;
  const uchar *const_tbyte_ptr;
} xcharp_u;

#endif /* UNICODE && WIN32 */

#endif /* HEADER_CURL_MULTIBYTE_H */
