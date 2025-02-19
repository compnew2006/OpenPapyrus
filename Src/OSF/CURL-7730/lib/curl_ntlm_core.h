#ifndef HEADER_CURL_NTLM_CORE_H
#define HEADER_CURL_NTLM_CORE_H
/***************************************************************************
*                                  _   _ ____  _
*  Project                     ___| | | |  _ \| |
*                             / __| | | | |_) | |
*                            | (__| |_| |  _ <| |___
*                             \___|\___/|_| \_\_____|
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

//#include "curl_setup.h"
#if defined(USE_CURL_NTLM_CORE)
/* If NSS is the first available SSL backend (see order in curl_ntlm_core.c)
   then it must be initialized to be used by NTLM. */
#if !defined(USE_OPENSSL) && !defined(USE_WOLFSSL) && !defined(USE_GNUTLS_NETTLE) && !defined(USE_GNUTLS) && defined(USE_NSS)
	#define NTLM_NEEDS_NSS_INIT
#endif
#if defined(USE_OPENSSL) || defined(USE_WOLFSSL)
#ifdef USE_WOLFSSL
	#include <wolfssl/options.h>
#endif
	#include <openssl/ssl.h>
#endif
/* Define USE_NTRESPONSES in order to make the type-3 message include the NT response message. */
#define USE_NTRESPONSES
/* Define USE_NTLM2SESSION in order to make the type-3 message include the
   NTLM2Session response message, requires USE_NTRESPONSES defined to 1 and
   MD5 support */
#if defined(USE_NTRESPONSES) && !defined(CURL_DISABLE_CRYPTO_AUTH)
	#define USE_NTLM2SESSION
#endif
/* Define USE_NTLM_V2 in order to allow the type-3 message to include the
   LMv2 and NTLMv2 response messages, requires USE_NTRESPONSES defined to 1
   and support for 64-bit integers. */
#if defined(USE_NTRESPONSES) && (CURL_SIZEOF_CURL_OFF_T > 4)
	#define USE_NTLM_V2
#endif
void Curl_ntlm_core_lm_resp(const uchar * keys, const uchar * plaintext, uchar * results);
CURLcode Curl_ntlm_core_mk_lm_hash(struct Curl_easy * data, const char * password, uchar * lmbuffer /* 21 bytes */);
#ifdef USE_NTRESPONSES
CURLcode Curl_ntlm_core_mk_nt_hash(struct Curl_easy * data, const char * password, uchar * ntbuffer /* 21 bytes */);

#if defined(USE_NTLM_V2) && !defined(USE_WINDOWS_SSPI)

CURLcode Curl_hmac_md5(const uchar * key, unsigned int keylen, const uchar * data, unsigned int datalen, uchar * output);
CURLcode Curl_ntlm_core_mk_ntlmv2_hash(const char * user, size_t userlen, const char * domain, size_t domlen, uchar * ntlmhash, uchar * ntlmv2hash);
CURLcode  Curl_ntlm_core_mk_ntlmv2_resp(uchar * ntlmv2hash, uchar * challenge_client, struct ntlmdata * ntlm, uchar ** ntresp, unsigned int * ntresp_len);
CURLcode  Curl_ntlm_core_mk_lmv2_resp(uchar * ntlmv2hash, uchar * challenge_client, uchar * challenge_server, uchar * lmresp);

#endif /* USE_NTLM_V2 && !USE_WINDOWS_SSPI */

#endif /* USE_NTRESPONSES */

#endif /* USE_CURL_NTLM_CORE */

#endif /* HEADER_CURL_NTLM_CORE_H */
