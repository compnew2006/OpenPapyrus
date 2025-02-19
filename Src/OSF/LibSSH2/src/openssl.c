/* Copyright (C) 2009, 2010 Simon Josefsson
 * Copyright (C) 2006, 2007 The Written Word, Inc.  All rights reserved.
 * Copyright (c) 2004-2006, Sara Golemon <sarag@libssh2.org>
 *
 * Author: Simon Josefsson
 *
 * Redistribution and use in source and binary forms,
 * with or without modification, are permitted provided
 * that the following conditions are met:
 *
 * Redistributions of source code must retain the above
 * copyright notice, this list of conditions and the
 * following disclaimer.
 *
 * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials
 * provided with the distribution.
 *
 * Neither the name of the copyright holder nor the names
 * of any other contributors may be used to endorse or
 * promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */
#include "libssh2_priv.h"
#pragma hdrstop

#ifdef LIBSSH2_OPENSSL /* compile only if we build with openssl */

//#include <string.h>
#ifndef EVP_MAX_BLOCK_LENGTH
	#define EVP_MAX_BLOCK_LENGTH 32
#endif

int _libssh2_rsa_new(libssh2_rsa_ctx ** rsa, const uchar * edata, ulong elen, const uchar * ndata,
    ulong nlen, const uchar * ddata, ulong dlen, const uchar * pdata, ulong plen,
    const uchar * qdata, ulong qlen, const uchar * e1data, ulong e1len, const uchar * e2data,
    ulong e2len, const uchar * coeffdata, ulong coefflen)
{
	BIGNUM * n;
	BIGNUM * d = 0;
	BIGNUM * p = 0;
	BIGNUM * q = 0;
	BIGNUM * dmp1 = 0;
	BIGNUM * dmq1 = 0;
	BIGNUM * iqmp = 0;
	BIGNUM * e = BN_new();
	BN_bin2bn(edata, elen, e);
	n = BN_new();
	BN_bin2bn(ndata, nlen, n);
	if(ddata) {
		d = BN_new();
		BN_bin2bn(ddata, dlen, d);
		p = BN_new();
		BN_bin2bn(pdata, plen, p);
		q = BN_new();
		BN_bin2bn(qdata, qlen, q);
		dmp1 = BN_new();
		BN_bin2bn(e1data, e1len, dmp1);
		dmq1 = BN_new();
		BN_bin2bn(e2data, e2len, dmq1);
		iqmp = BN_new();
		BN_bin2bn(coeffdata, coefflen, iqmp);
	}
	*rsa = RSA_new();
#ifdef HAVE_OPAQUE_STRUCTS
	RSA_set0_key(*rsa, n, e, d);
#else
	(*rsa)->e = e;
	(*rsa)->n = n;
#endif
#ifdef HAVE_OPAQUE_STRUCTS
	RSA_set0_factors(*rsa, p, q);
#else
	(*rsa)->p = p;
	(*rsa)->q = q;
#endif
#ifdef HAVE_OPAQUE_STRUCTS
	RSA_set0_crt_params(*rsa, dmp1, dmq1, iqmp);
#else
	(*rsa)->dmp1 = dmp1;
	(*rsa)->dmq1 = dmq1;
	(*rsa)->iqmp = iqmp;
#endif
	return 0;
}

int _libssh2_rsa_sha1_verify(libssh2_rsa_ctx * rsactx, const uchar * sig, ulong sig_len, const uchar * m, ulong m_len)
{
	uchar hash[SHA_DIGEST_LENGTH];
	int ret;
	if(_libssh2_sha1(m, m_len, hash))
		return -1; /* failure */
	ret = RSA_verify(NID_sha1, hash, SHA_DIGEST_LENGTH, (uchar *)sig, sig_len, rsactx);
	return (ret == 1) ? 0 : -1;
}

#if LIBSSH2_DSA
int _libssh2_dsa_new(libssh2_dsa_ctx ** dsactx, const uchar * p, ulong p_len,
    const uchar * q, ulong q_len, const uchar * g, ulong g_len, const uchar * y,
    ulong y_len, const uchar * x, ulong x_len)
{
	BIGNUM * p_bn;
	BIGNUM * q_bn;
	BIGNUM * g_bn;
	BIGNUM * pub_key;
	BIGNUM * priv_key = NULL;
	p_bn = BN_new();
	BN_bin2bn(p, p_len, p_bn);
	q_bn = BN_new();
	BN_bin2bn(q, q_len, q_bn);
	g_bn = BN_new();
	BN_bin2bn(g, g_len, g_bn);
	pub_key = BN_new();
	BN_bin2bn(y, y_len, pub_key);
	if(x_len) {
		priv_key = BN_new();
		BN_bin2bn(x, x_len, priv_key);
	}
	*dsactx = (libssh2_dsa_ctx *)DSA_new();
#ifdef HAVE_OPAQUE_STRUCTS
	DSA_set0_pqg((DSA *)*dsactx, p_bn, q_bn, g_bn);
#else
	(*dsactx)->p = p_bn;
	(*dsactx)->g = g_bn;
	(*dsactx)->q = q_bn;
#endif

#ifdef HAVE_OPAQUE_STRUCTS
	DSA_set0_key((DSA *)*dsactx, pub_key, priv_key);
#else
	(*dsactx)->pub_key = pub_key;
	(*dsactx)->priv_key = priv_key;
#endif
	return 0;
}

int _libssh2_dsa_sha1_verify(libssh2_dsa_ctx * dsactx, const uchar * sig, const uchar * m, ulong m_len)
{
	uchar hash[SHA_DIGEST_LENGTH];
	DSA_SIG * dsasig;
	BIGNUM * s;
	int ret = -1;
	BIGNUM * r = BN_new();
	BN_bin2bn(sig, 20, r);
	s = BN_new();
	BN_bin2bn(sig + 20, 20, s);
	dsasig = DSA_SIG_new();
#ifdef HAVE_OPAQUE_STRUCTS
	DSA_SIG_set0(dsasig, r, s);
#else
	dsasig->r = r;
	dsasig->s = s;
#endif
	if(!_libssh2_sha1(m, m_len, hash))
		/* _libssh2_sha1() succeeded */
		ret = DSA_do_verify(hash, SHA_DIGEST_LENGTH, dsasig, (DSA *)dsactx);
	DSA_SIG_free(dsasig);
	return (ret == 1) ? 0 : -1;
}

#endif /* LIBSSH_DSA */

int _libssh2_cipher_init(_libssh2_cipher_ctx * h, _libssh2_cipher_type(algo), uchar * iv, uchar * secret, int encrypt)
{
#ifdef HAVE_OPAQUE_STRUCTS
	*h = EVP_CIPHER_CTX_new();
	return !EVP_CipherInit(*h, algo(), secret, iv, encrypt);
#else
	EVP_CIPHER_CTX_init(h);
	return !EVP_CipherInit(h, algo(), secret, iv, encrypt);
#endif
}

int _libssh2_cipher_crypt(_libssh2_cipher_ctx * ctx, _libssh2_cipher_type(algo), int encrypt, uchar * block, size_t blocksize)
{
	uchar buf[EVP_MAX_BLOCK_LENGTH];
	int ret;
	(void)algo;
	(void)encrypt;
#ifdef HAVE_OPAQUE_STRUCTS
	ret = EVP_Cipher(*ctx, buf, block, blocksize);
#else
	ret = EVP_Cipher(ctx, buf, block, blocksize);
#endif
	if(ret == 1) {
		memcpy(block, buf, blocksize);
	}
	return ret == 1 ? 0 : 1;
}

#if LIBSSH2_AES_CTR && !defined(HAVE_EVP_AES_128_CTR)

#include <openssl/aes.h>
#include <openssl/evp.h>

typedef struct {
	AES_KEY key;
	EVP_CIPHER_CTX * aes_ctx;
	uchar ctr[AES_BLOCK_SIZE];
} aes_ctr_ctx;

static int aes_ctr_init(EVP_CIPHER_CTX * ctx, const uchar * key, const uchar * iv, int enc)         /* init key */
{
	// variable "c" is leaked from this scope, but is later freed in aes_ctr_cleanup
	aes_ctr_ctx * c;
	const EVP_CIPHER * aes_cipher;
	(void)enc;
	switch(EVP_CIPHER_CTX_key_length(ctx)) {
		case 16: aes_cipher = EVP_aes_128_ecb(); break;
		case 24: aes_cipher = EVP_aes_192_ecb(); break;
		case 32: aes_cipher = EVP_aes_256_ecb(); break;
		default: return 0;
	}
	c = static_cast<aes_ctr_ctx *>(SAlloc::M(sizeof(*c)));
	if(c == NULL)
		return 0;
#ifdef HAVE_OPAQUE_STRUCTS
	c->aes_ctx = EVP_CIPHER_CTX_new();
#else
	c->aes_ctx = SAlloc::M(sizeof(EVP_CIPHER_CTX));
#endif
	if(c->aes_ctx == NULL) {
		SAlloc::F(c);
		return 0;
	}
	if(EVP_EncryptInit(c->aes_ctx, aes_cipher, key, NULL) != 1) {
#ifdef HAVE_OPAQUE_STRUCTS
		EVP_CIPHER_CTX_free(c->aes_ctx);
#else
		SAlloc::F(c->aes_ctx);
#endif
		SAlloc::F(c);
		return 0;
	}
	EVP_CIPHER_CTX_set_padding(c->aes_ctx, 0);
	memcpy(c->ctr, iv, AES_BLOCK_SIZE);
	EVP_CIPHER_CTX_set_app_data(ctx, c);
	return 1;
}

static int aes_ctr_do_cipher(EVP_CIPHER_CTX * ctx, uchar * out, const uchar * in, size_t inl) /* encrypt/decrypt data */
{
	aes_ctr_ctx * c = static_cast<aes_ctr_ctx *>(EVP_CIPHER_CTX_get_app_data(ctx));
	uchar b1[AES_BLOCK_SIZE];
	size_t i = 0;
	int outlen = 0;
	if(inl != 16) /* libssh2 only ever encrypt one block */
		return 0;
	if(c == NULL) {
		return 0;
	}
	/*
	   To encrypt a packet P=P1||P2||...||Pn (where P1, P2, ..., Pn are each
	   blocks of length L), the encryptor first encrypts <X> with <cipher>
	   to obtain a block B1.  The block B1 is then XORed with P1 to generate
	   the ciphertext block C1.  The counter X is then incremented
	 */
	if(EVP_EncryptUpdate(c->aes_ctx, b1, &outlen, c->ctr, AES_BLOCK_SIZE) != 1) {
		return 0;
	}
	for(i = 0; i < 16; i++)
		*out++ = *in++ ^ b1[i];
	i = 15;
	while(c->ctr[i]++ == 0xFF) {
		if(i == 0)
			break;
		i--;
	}
	return 1;
}

static int aes_ctr_cleanup(EVP_CIPHER_CTX * ctx) /* cleanup ctx */
{
	aes_ctr_ctx * c = static_cast<aes_ctr_ctx *>(EVP_CIPHER_CTX_get_app_data(ctx));
	if(c == NULL) {
		return 1;
	}
	if(c->aes_ctx != NULL) {
#ifdef HAVE_OPAQUE_STRUCTS
		EVP_CIPHER_CTX_free(c->aes_ctx);
#else
		_libssh2_cipher_dtor(c->aes_ctx);
		SAlloc::F(c->aes_ctx);
#endif
	}
	SAlloc::F(c);
	return 1;
}

static const EVP_CIPHER * make_ctr_evp(size_t keylen, EVP_CIPHER * aes_ctr_cipher, int type)
{
#ifdef HAVE_OPAQUE_STRUCTS
	aes_ctr_cipher = EVP_CIPHER_meth_new(type, 16, keylen);
	if(aes_ctr_cipher) {
		EVP_CIPHER_meth_set_iv_length(aes_ctr_cipher, 16);
		EVP_CIPHER_meth_set_init(aes_ctr_cipher, aes_ctr_init);
		EVP_CIPHER_meth_set_do_cipher(aes_ctr_cipher, aes_ctr_do_cipher);
		EVP_CIPHER_meth_set_cleanup(aes_ctr_cipher, aes_ctr_cleanup);
	}
#else
	aes_ctr_cipher->nid = type;
	aes_ctr_cipher->block_size = 16;
	aes_ctr_cipher->key_len = keylen;
	aes_ctr_cipher->iv_len = 16;
	aes_ctr_cipher->init = aes_ctr_init;
	aes_ctr_cipher->do_cipher = aes_ctr_do_cipher;
	aes_ctr_cipher->cleanup = aes_ctr_cleanup;
#endif

	return aes_ctr_cipher;
}

const EVP_CIPHER * _libssh2_EVP_aes_128_ctr(void)
{
#ifdef HAVE_OPAQUE_STRUCTS
	static EVP_CIPHER * aes_ctr_cipher;
	return !aes_ctr_cipher ? make_ctr_evp(16, aes_ctr_cipher, NID_aes_128_ctr) : aes_ctr_cipher;
#else
	static EVP_CIPHER aes_ctr_cipher;
	return !aes_ctr_cipher.key_len ? make_ctr_evp(16, &aes_ctr_cipher, 0) : &aes_ctr_cipher;
#endif
}

const EVP_CIPHER * _libssh2_EVP_aes_192_ctr(void)
{
#ifdef HAVE_OPAQUE_STRUCTS
	static EVP_CIPHER * aes_ctr_cipher;
	return !aes_ctr_cipher ? make_ctr_evp(24, aes_ctr_cipher, NID_aes_192_ctr) : aes_ctr_cipher;
#else
	static EVP_CIPHER aes_ctr_cipher;
	return !aes_ctr_cipher.key_len ? make_ctr_evp(24, &aes_ctr_cipher, 0) : &aes_ctr_cipher;
#endif
}

const EVP_CIPHER * _libssh2_EVP_aes_256_ctr(void)
{
#ifdef HAVE_OPAQUE_STRUCTS
	static EVP_CIPHER * aes_ctr_cipher;
	return !aes_ctr_cipher ? make_ctr_evp(32, aes_ctr_cipher, NID_aes_256_ctr) : aes_ctr_cipher;
#else
	static EVP_CIPHER aes_ctr_cipher;
	return !aes_ctr_cipher.key_len ? make_ctr_evp(32, &aes_ctr_cipher, 0) : &aes_ctr_cipher;
#endif
}

void _libssh2_init_aes_ctr(void)
{
	_libssh2_EVP_aes_128_ctr();
	_libssh2_EVP_aes_192_ctr();
	_libssh2_EVP_aes_256_ctr();
}

#else
void _libssh2_init_aes_ctr(void) {
}

#endif /* LIBSSH2_AES_CTR */

/* TODO: Optionally call a passphrase callback specified by the
 * calling program
 */
static int passphrase_cb(char * buf, int size, int rwflag, const char * passphrase)
{
	int passphrase_len = strlen(passphrase);
	(void)rwflag;
	if(passphrase_len > (size - 1)) {
		passphrase_len = size - 1;
	}
	memcpy(buf, passphrase, passphrase_len);
	buf[passphrase_len] = '\0';
	return passphrase_len;
}

typedef void * (*pem_read_bio_func)(BIO *, void **, pem_password_cb *, void * u);

static int read_private_key_from_memory(void ** key_ctx, pem_read_bio_func read_private_key,
    const char * filedata, size_t filedata_len, const uchar * passphrase)
{
	BIO * bp;
	*key_ctx = NULL;
	bp = BIO_new_mem_buf((char *)filedata, filedata_len);
	if(!bp) {
		return -1;
	}
	*key_ctx = read_private_key(bp, NULL, (pem_password_cb*)passphrase_cb, (void *)passphrase);
	BIO_free(bp);
	return (*key_ctx) ? 0 : -1;
}

static int read_private_key_from_file(void ** key_ctx, pem_read_bio_func read_private_key, const char * filename, const uchar * passphrase)
{
	BIO * bp;
	*key_ctx = NULL;
	bp = BIO_new_file(filename, "r");
	if(!bp) {
		return -1;
	}
	*key_ctx = read_private_key(bp, NULL, (pem_password_cb*)passphrase_cb, (void *)passphrase);
	BIO_free(bp);
	return (*key_ctx) ? 0 : -1;
}

int _libssh2_rsa_new_private_frommemory(libssh2_rsa_ctx ** rsa, LIBSSH2_SESSION * session, const char * filedata, size_t filedata_len, const uchar * passphrase)
{
	pem_read_bio_func read_rsa = (pem_read_bio_func) &PEM_read_bio_RSAPrivateKey;
	(void)session;
	_libssh2_init_if_needed();
	return read_private_key_from_memory((void **)rsa, read_rsa, filedata, filedata_len, passphrase);
}

int _libssh2_rsa_new_private(libssh2_rsa_ctx ** rsa, LIBSSH2_SESSION * session, const char * filename, const uchar * passphrase)
{
	pem_read_bio_func read_rsa = (pem_read_bio_func) &PEM_read_bio_RSAPrivateKey;
	(void)session;
	_libssh2_init_if_needed();
	return read_private_key_from_file((void **)rsa, read_rsa, filename, passphrase);
}

#if LIBSSH2_DSA
int _libssh2_dsa_new_private_frommemory(libssh2_dsa_ctx ** dsa, LIBSSH2_SESSION * session, const char * filedata, size_t filedata_len, const uchar * passphrase)
{
	pem_read_bio_func read_dsa = (pem_read_bio_func) &PEM_read_bio_DSAPrivateKey;
	(void)session;
	_libssh2_init_if_needed();
	return read_private_key_from_memory((void **)dsa, read_dsa, filedata, filedata_len, passphrase);
}

int _libssh2_dsa_new_private(libssh2_dsa_ctx ** dsa, LIBSSH2_SESSION * session, const char * filename, const uchar * passphrase)
{
	pem_read_bio_func read_dsa = (pem_read_bio_func) &PEM_read_bio_DSAPrivateKey;
	(void)session;
	_libssh2_init_if_needed();
	return read_private_key_from_file((void **)dsa, read_dsa, filename, passphrase);
}

#endif /* LIBSSH_DSA */

int _libssh2_rsa_sha1_sign(LIBSSH2_SESSION * session, libssh2_rsa_ctx * rsactx, const uchar * hash, size_t hash_len, uchar ** signature, size_t * signature_len)
{
	int ret;
	uint sig_len = RSA_size(rsactx);
	uchar * sig = (uchar *)LIBSSH2_ALLOC(session, sig_len);
	if(!sig) {
		return -1;
	}
	ret = RSA_sign(NID_sha1, hash, hash_len, sig, &sig_len, rsactx);
	if(!ret) {
		LIBSSH2_FREE(session, sig);
		return -1;
	}
	*signature = sig;
	*signature_len = sig_len;
	return 0;
}

#if LIBSSH2_DSA
int _libssh2_dsa_sha1_sign(libssh2_dsa_ctx * dsactx, const uchar * hash, ulong hash_len, uchar * signature)
{
	DSA_SIG * sig;
	const BIGNUM * r;
	const BIGNUM * s;
	int r_len, s_len;
	(void)hash_len;
	sig = DSA_do_sign(hash, SHA_DIGEST_LENGTH, (DSA *)dsactx);
	if(!sig) {
		return -1;
	}
#ifdef HAVE_OPAQUE_STRUCTS
	DSA_SIG_get0(sig, &r, &s);
#else
	r = sig->r;
	s = sig->s;
#endif
	r_len = BN_num_bytes(r);
	if(r_len < 1 || r_len > 20) {
		DSA_SIG_free(sig);
		return -1;
	}
	s_len = BN_num_bytes(s);
	if(s_len < 1 || s_len > 20) {
		DSA_SIG_free(sig);
		return -1;
	}
	memzero(signature, 40);
	BN_bn2bin(r, signature + (20 - r_len));
	BN_bn2bin(s, signature + 20 + (20 - s_len));
	DSA_SIG_free(sig);
	return 0;
}

#endif /* LIBSSH_DSA */

int _libssh2_sha1_init(libssh2_sha1_ctx * ctx)
{
#ifdef HAVE_OPAQUE_STRUCTS
	*ctx = EVP_MD_CTX_new();
	if(*ctx == NULL)
		return 0;
	if(EVP_DigestInit(*ctx, EVP_get_digestbyname("sha1")))
		return 1;
	EVP_MD_CTX_free(*ctx);
	*ctx = NULL;
	return 0;
#else
	EVP_MD_CTX_init(ctx);
	return EVP_DigestInit(ctx, EVP_get_digestbyname("sha1"));
#endif
}

int _libssh2_sha1(const uchar * message, ulong len, uchar * out)
{
#ifdef HAVE_OPAQUE_STRUCTS
	EVP_MD_CTX * ctx = EVP_MD_CTX_new();
	if(ctx == NULL)
		return 1; /* error */
	if(EVP_DigestInit(ctx, EVP_get_digestbyname("sha1"))) {
		EVP_DigestUpdate(ctx, message, len);
		EVP_DigestFinal(ctx, out, NULL);
		EVP_MD_CTX_free(ctx);
		return 0; /* success */
	}
	EVP_MD_CTX_free(ctx);
#else
	EVP_MD_CTX ctx;
	EVP_MD_CTX_init(&ctx);
	if(EVP_DigestInit(&ctx, EVP_get_digestbyname("sha1"))) {
		EVP_DigestUpdate(&ctx, message, len);
		EVP_DigestFinal(&ctx, out, NULL);
		return 0; /* success */
	}
#endif
	return 1; /* error */
}

int _libssh2_sha256_init(libssh2_sha256_ctx * ctx)
{
#ifdef HAVE_OPAQUE_STRUCTS
	*ctx = EVP_MD_CTX_new();
	if(*ctx == NULL)
		return 0;
	if(EVP_DigestInit(*ctx, EVP_get_digestbyname("sha256")))
		return 1;
	EVP_MD_CTX_free(*ctx);
	*ctx = NULL;
	return 0;
#else
	EVP_MD_CTX_init(ctx);
	return EVP_DigestInit(ctx, EVP_get_digestbyname("sha256"));
#endif
}

int _libssh2_sha256(const uchar * message, ulong len, uchar * out)
{
#ifdef HAVE_OPAQUE_STRUCTS
	EVP_MD_CTX * ctx = EVP_MD_CTX_new();
	if(ctx == NULL)
		return 1; /* error */
	if(EVP_DigestInit(ctx, EVP_get_digestbyname("sha256"))) {
		EVP_DigestUpdate(ctx, message, len);
		EVP_DigestFinal(ctx, out, NULL);
		EVP_MD_CTX_free(ctx);
		return 0; /* success */
	}
	EVP_MD_CTX_free(ctx);
#else
	EVP_MD_CTX ctx;
	EVP_MD_CTX_init(&ctx);
	if(EVP_DigestInit(&ctx, EVP_get_digestbyname("sha256"))) {
		EVP_DigestUpdate(&ctx, message, len);
		EVP_DigestFinal(&ctx, out, NULL);
		return 0; /* success */
	}
#endif
	return 1; /* error */
}

int _libssh2_md5_init(libssh2_md5_ctx * ctx)
{
#ifdef HAVE_OPAQUE_STRUCTS
	*ctx = EVP_MD_CTX_new();
	if(*ctx == NULL)
		return 0;
	if(EVP_DigestInit(*ctx, EVP_get_digestbyname("md5")))
		return 1;
	EVP_MD_CTX_free(*ctx);
	*ctx = NULL;
	return 0;
#else
	EVP_MD_CTX_init(ctx);
	return EVP_DigestInit(ctx, EVP_get_digestbyname("md5"));
#endif
}

static uchar * FASTCALL write_bn(uchar * buf, const BIGNUM * bn, int bn_bytes)
{
	uchar * p = buf;
	/* Left space for bn size which will be written below. */
	p += 4;
	*p = 0;
	BN_bn2bin(bn, p + 1);
	if(!(*(p + 1) & 0x80)) {
		memmove(p, p + 1, --bn_bytes);
	}
	_libssh2_htonu32(p - 4, bn_bytes); /* Post write bn size. */
	return p + bn_bytes;
}

static uchar * gen_publickey_from_rsa(LIBSSH2_SESSION * session, RSA * rsa, size_t * key_len)
{
	int e_bytes, n_bytes;
	ulong len;
	uchar * key;
	uchar * p;
	const BIGNUM * e;
	const BIGNUM * n;
#ifdef HAVE_OPAQUE_STRUCTS
	RSA_get0_key(rsa, &n, &e, NULL);
#else
	e = rsa->e;
	n = rsa->n;
#endif
	e_bytes = BN_num_bytes(e) + 1;
	n_bytes = BN_num_bytes(n) + 1;
	/* Key form is "ssh-rsa" + e + n. */
	len = 4 + 7 + 4 + e_bytes + 4 + n_bytes;
	key = (uchar *)LIBSSH2_ALLOC(session, len);
	if(key == NULL) {
		return NULL;
	}
	/* Process key encoding. */
	p = key;
	_libssh2_htonu32(p, 7); /* Key type. */
	p += 4;
	memcpy(p, "ssh-rsa", 7);
	p += 7;
	p = write_bn(p, e, e_bytes);
	p = write_bn(p, n, n_bytes);
	*key_len = (size_t)(p - key);
	return key;
}

#if LIBSSH2_DSA
static uchar * gen_publickey_from_dsa(LIBSSH2_SESSION* session, DSA * dsa, size_t * key_len)
{
	int p_bytes, q_bytes, g_bytes, k_bytes;
	ulong len;
	uchar * key;
	uchar * p;
	const BIGNUM * p_bn;
	const BIGNUM * q;
	const BIGNUM * g;
	const BIGNUM * pub_key;
#ifdef HAVE_OPAQUE_STRUCTS
	DSA_get0_pqg(dsa, &p_bn, &q, &g);
#else
	p_bn = dsa->p;
	q = dsa->q;
	g = dsa->g;
#endif

#ifdef HAVE_OPAQUE_STRUCTS
	DSA_get0_key(dsa, &pub_key, NULL);
#else
	pub_key = dsa->pub_key;
#endif
	p_bytes = BN_num_bytes(p_bn) + 1;
	q_bytes = BN_num_bytes(q) + 1;
	g_bytes = BN_num_bytes(g) + 1;
	k_bytes = BN_num_bytes(pub_key) + 1;

	/* Key form is "ssh-dss" + p + q + g + pub_key. */
	len = 4 + 7 + 4 + p_bytes + 4 + q_bytes + 4 + g_bytes + 4 + k_bytes;

	key = (uchar *)LIBSSH2_ALLOC(session, len);
	if(key == NULL) {
		return NULL;
	}

	/* Process key encoding. */
	p = key;

	_libssh2_htonu32(p, 7); /* Key type. */
	p += 4;
	memcpy(p, "ssh-dss", 7);
	p += 7;
	p = write_bn(p, p_bn, p_bytes);
	p = write_bn(p, q, q_bytes);
	p = write_bn(p, g, g_bytes);
	p = write_bn(p, pub_key, k_bytes);
	*key_len = (size_t)(p - key);
	return key;
}

#endif /* LIBSSH_DSA */

static int gen_publickey_from_rsa_evp(LIBSSH2_SESSION * session, uchar ** method,
    size_t * method_len, uchar ** pubkeydata, size_t * pubkeydata_len, EVP_PKEY * pk)
{
	RSA * rsa = NULL;
	uchar * key;
	uchar * method_buf = NULL;
	size_t key_len;
	_libssh2_debug(session, LIBSSH2_TRACE_AUTH, "Computing public key from RSA private key envelop");
	rsa = EVP_PKEY_get1_RSA(pk);
	if(rsa == NULL) {
		/* Assume memory allocation error... what else could it be ? */
		goto __alloc_error;
	}
	method_buf = (uchar *)LIBSSH2_ALLOC(session, 7); /* ssh-rsa. */
	if(method_buf == NULL) {
		goto __alloc_error;
	}
	key = gen_publickey_from_rsa(session, rsa, &key_len);
	if(key == NULL) {
		goto __alloc_error;
	}
	RSA_free(rsa);
	memcpy(method_buf, "ssh-rsa", 7);
	*method = method_buf;
	*method_len     = 7;
	*pubkeydata     = key;
	*pubkeydata_len = key_len;
	return 0;
__alloc_error:
	RSA_free(rsa);
	if(method_buf) {
		LIBSSH2_FREE(session, method_buf);
	}
	return _libssh2_error(session, LIBSSH2_ERROR_ALLOC, "Unable to allocate memory for private key data");
}

#if LIBSSH2_DSA
static int gen_publickey_from_dsa_evp(LIBSSH2_SESSION * session, uchar ** method, size_t * method_len, uchar ** pubkeydata, size_t * pubkeydata_len, EVP_PKEY * pk)
{
	DSA*           dsa = NULL;
	uchar * key;
	uchar * method_buf = NULL;
	size_t key_len;
	_libssh2_debug(session, LIBSSH2_TRACE_AUTH, "Computing public key from DSA private key envelop");
	dsa = EVP_PKEY_get1_DSA(pk);
	if(dsa == NULL) {
		/* Assume memory allocation error... what else could it be ? */
		goto __alloc_error;
	}
	method_buf = (uchar *)LIBSSH2_ALLOC(session, 7); /* ssh-dss. */
	if(method_buf == NULL) {
		goto __alloc_error;
	}
	key = gen_publickey_from_dsa(session, dsa, &key_len);
	if(key == NULL) {
		goto __alloc_error;
	}
	DSA_free(dsa);
	memcpy(method_buf, "ssh-dss", 7);
	*method = method_buf;
	*method_len     = 7;
	*pubkeydata     = key;
	*pubkeydata_len = key_len;
	return 0;
__alloc_error:
	DSA_free(dsa);
	if(method_buf) {
		LIBSSH2_FREE(session, method_buf);
	}
	return _libssh2_error(session, LIBSSH2_ERROR_ALLOC, "Unable to allocate memory for private key data");
}

#endif /* LIBSSH_DSA */

int _libssh2_pub_priv_keyfile(LIBSSH2_SESSION * session, uchar ** method, size_t * method_len,
    uchar ** pubkeydata, size_t * pubkeydata_len, const char * privatekey, const char * passphrase)
{
	int st;
	BIO * bp;
	EVP_PKEY * pk;
	int pktype;
	_libssh2_debug(session, LIBSSH2_TRACE_AUTH, "Computing public key from private key file: %s", privatekey);
	bp = BIO_new_file(privatekey, "r");
	if(bp == NULL) {
		return _libssh2_error(session, LIBSSH2_ERROR_FILE, "Unable to extract public key from private key file: Unable to open private key file");
	}
	if(!EVP_get_cipherbyname("des")) {
		/* If this cipher isn't loaded it's a pretty good indication that none
		 * are.  I have *NO DOUBT* that there's a better way to deal with this
		 * ($#&%#$(%$#( Someone buy me an OpenSSL manual and I'll read up on
		 * it.
		 */
		OpenSSL_add_all_ciphers();
	}
	BIO_reset(bp);
	pk = PEM_read_bio_PrivateKey(bp, NULL, NULL, (void *)passphrase);
	BIO_free(bp);
	if(pk == NULL) {
		return _libssh2_error(session, LIBSSH2_ERROR_FILE, "Unable to extract public key from private key file: Wrong passphrase or invalid/unrecognized private key file format");
	}
#ifdef HAVE_OPAQUE_STRUCTS
	pktype = EVP_PKEY_id(pk);
#else
	pktype = pk->type;
#endif
	switch(pktype) {
		case EVP_PKEY_RSA:
		    st = gen_publickey_from_rsa_evp(session, method, method_len, pubkeydata, pubkeydata_len, pk);
		    break;
#if LIBSSH2_DSA
		case EVP_PKEY_DSA:
		    st = gen_publickey_from_dsa_evp(session, method, method_len, pubkeydata, pubkeydata_len, pk);
		    break;
#endif /* LIBSSH_DSA */
		default:
		    st = _libssh2_error(session, LIBSSH2_ERROR_FILE, "Unable to extract public key from private key file: Unsupported private key file format");
		    break;
	}
	EVP_PKEY_free(pk);
	return st;
}

int _libssh2_pub_priv_keyfilememory(LIBSSH2_SESSION * session, uchar ** method, size_t * method_len,
    uchar ** pubkeydata, size_t * pubkeydata_len, const char * privatekeydata, size_t privatekeydata_len, const char * passphrase)
{
	int st;
	BIO*      bp;
	EVP_PKEY* pk;
	int pktype;
	_libssh2_debug(session, LIBSSH2_TRACE_AUTH, "Computing public key from private key.");
	bp = BIO_new_mem_buf((char *)privatekeydata, privatekeydata_len);
	if(!bp) {
		return -1;
	}
	if(!EVP_get_cipherbyname("des")) {
		/* If this cipher isn't loaded it's a pretty good indication that none
		 * are.  I have *NO DOUBT* that there's a better way to deal with this
		 * ($#&%#$(%$#( Someone buy me an OpenSSL manual and I'll read up on
		 * it.
		 */
		OpenSSL_add_all_ciphers();
	}
	BIO_reset(bp);
	pk = PEM_read_bio_PrivateKey(bp, NULL, NULL, (void *)passphrase);
	BIO_free(bp);
	if(pk == NULL) {
		return _libssh2_error(session, LIBSSH2_ERROR_FILE, "Unable to extract public key from private key file: Wrong passphrase or invalid/unrecognized private key file format");
	}
#ifdef HAVE_OPAQUE_STRUCTS
	pktype = EVP_PKEY_id(pk);
#else
	pktype = pk->type;
#endif
	switch(pktype) {
		case EVP_PKEY_RSA:
		    st = gen_publickey_from_rsa_evp(session, method, method_len, pubkeydata, pubkeydata_len, pk);
		    break;
#if LIBSSH2_DSA
		case EVP_PKEY_DSA:
		    st = gen_publickey_from_dsa_evp(session, method, method_len, pubkeydata, pubkeydata_len, pk);
		    break;
#endif /* LIBSSH_DSA */
		default:
		    st = _libssh2_error(session, LIBSSH2_ERROR_FILE, "Unable to extract public key from private key file: Unsupported private key file format");
		    break;
	}
	EVP_PKEY_free(pk);
	return st;
}

#endif /* LIBSSH2_OPENSSL */
