/*
 * Copyright 1995-2017 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at  https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal/nelem.h"

#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/rand.h>
#include <openssl/err.h>

#include "testutil.h"

#define NUM_BITS        (BN_BITS2 * 4)

#define BN_print_var(v) test_output_bignum(#v, v)

/*
 * Test that r == 0 in test_exp_mod_zero(). Returns one on success,
 * returns zero and prints debug output otherwise.
 */
static int a_is_zero_mod_one(const char *method, const BIGNUM *r,
                             const BIGNUM *a)
{
    if (!BN_is_zero(r)) {
        TEST_error("%s failed: a ** 0 mod 1 = r (should be 0)", method);
        BN_print_var(a);
        BN_print_var(r);
        return 0;
    }
    return 1;
}

/*
 * test_mod_exp_zero tests that x**0 mod 1 == 0. It returns zero on success.
 */
static int test_mod_exp_zero(void)
{
    BIGNUM *a = NULL, *p = NULL, *m = NULL;
    BIGNUM *r = NULL;
    BN_ULONG one_word = 1;
    BN_CTX *ctx = BN_CTX_new();
    int ret = 1, failed = 0;

    if (!TEST_ptr(m = BN_new())
        || !TEST_ptr(a = BN_new())
        || !TEST_ptr(p = BN_new())
        || !TEST_ptr(r = BN_new()))
        goto err;

    BN_one(m);
    BN_one(a);
    BN_zero(p);

    if (!TEST_true(BN_rand(a, 1024, BN_RAND_TOP_ONE, BN_RAND_BOTTOM_ANY)))
        goto err;

    if (!TEST_true(BN_mod_exp(r, a, p, m, ctx)))
        goto err;

    if (!TEST_true(a_is_zero_mod_one("BN_mod_exp", r, a)))
        failed = 1;

    if (!TEST_true(BN_mod_exp_recp(r, a, p, m, ctx)))
        goto err;

    if (!TEST_true(a_is_zero_mod_one("BN_mod_exp_recp", r, a)))
        failed = 1;

    if (!TEST_true(BN_mod_exp_simple(r, a, p, m, ctx)))
        goto err;

    if (!TEST_true(a_is_zero_mod_one("BN_mod_exp_simple", r, a)))
        failed = 1;

    if (!TEST_true(BN_mod_exp_mont(r, a, p, m, ctx, NULL)))
        goto err;

    if (!TEST_true(a_is_zero_mod_one("BN_mod_exp_mont", r, a)))
        failed = 1;

    if (!TEST_true(BN_mod_exp_mont_consttime(r, a, p, m, ctx, NULL)))
        goto err;

    if (!TEST_true(a_is_zero_mod_one("BN_mod_exp_mont_consttime", r, a)))
        failed = 1;

    /*
     * A different codepath exists for single word multiplication
     * in non-constant-time only.
     */
    if (!TEST_true(BN_mod_exp_mont_word(r, one_word, p, m, ctx, NULL)))
        goto err;

    if (!TEST_BN_eq_zero(r)) {
        TEST_error("BN_mod_exp_mont_word failed: "
                   "1 ** 0 mod 1 = r (should be 0)");
        BN_print_var(r);
        goto err;
    }

    ret = !failed;
 err:
    BN_free(r);
    BN_free(a);
    BN_free(p);
    BN_free(m);
    BN_CTX_free(ctx);

    return ret;
}

static int test_mod_exp(int round)
{
    BN_CTX *ctx;
    unsigned char c;
    int ret = 0;
    BIGNUM *r_mont = NULL;
    BIGNUM *r_mont_const = NULL;
    BIGNUM *r_recp = NULL;
    BIGNUM *r_simple = NULL;
    BIGNUM *a = NULL;
    BIGNUM *b = NULL;
    BIGNUM *m = NULL;

    if (!TEST_ptr(ctx = BN_CTX_new()))
        goto err;

    if (!TEST_ptr(r_mont = BN_new())
        || !TEST_ptr(r_mont_const = BN_new())
        || !TEST_ptr(r_recp = BN_new())
        || !TEST_ptr(r_simple = BN_new())
        || !TEST_ptr(a = BN_new())
        || !TEST_ptr(b = BN_new())
        || !TEST_ptr(m = BN_new()))
        goto err;

    RAND_bytes(&c, 1);
    c = (c % BN_BITS) - BN_BITS2;
    BN_rand(a, NUM_BITS + c, BN_RAND_TOP_ONE, BN_RAND_BOTTOM_ANY);

    RAND_bytes(&c, 1);
    c = (c % BN_BITS) - BN_BITS2;
    BN_rand(b, NUM_BITS + c, BN_RAND_TOP_ONE, BN_RAND_BOTTOM_ANY);

    RAND_bytes(&c, 1);
    c = (c % BN_BITS) - BN_BITS2;
    BN_rand(m, NUM_BITS + c, BN_RAND_TOP_ONE, BN_RAND_BOTTOM_ODD);

    if (!TEST_true(BN_mod(a, a, m, ctx))
        || !TEST_true(BN_mod(b, b, m, ctx))
        || !TEST_true(BN_mod_exp_mont(r_mont, a, b, m, ctx, NULL))
        || !TEST_true(BN_mod_exp_recp(r_recp, a, b, m, ctx))
        || !TEST_true(BN_mod_exp_simple(r_simple, a, b, m, ctx))
        || !TEST_true(BN_mod_exp_mont_consttime(r_mont_const, a, b, m, ctx, NULL)))
        goto err;

    if (!TEST_BN_eq(r_simple, r_mont)
        || !TEST_BN_eq(r_simple, r_recp)
        || !TEST_BN_eq(r_simple, r_mont_const)) {
        if (BN_cmp(r_simple, r_mont) != 0)
            TEST_info("simple and mont results differ");
        if (BN_cmp(r_simple, r_mont_const) != 0)
            TEST_info("simple and mont const time results differ");
        if (BN_cmp(r_simple, r_recp) != 0)
            TEST_info("simple and recp results differ");

        BN_print_var(a);
        BN_print_var(b);
        BN_print_var(m);
        BN_print_var(r_simple);
        BN_print_var(r_recp);
        BN_print_var(r_mont);
        BN_print_var(r_mont_const);
        goto err;
    }

    ret = 1;
 err:
    BN_free(r_mont);
    BN_free(r_mont_const);
    BN_free(r_recp);
    BN_free(r_simple);
    BN_free(a);
    BN_free(b);
    BN_free(m);
    BN_CTX_free(ctx);

    return ret;
}

int setup_tests(void)
{
    ADD_TEST(test_mod_exp_zero);
    ADD_ALL_TESTS(test_mod_exp, 200);
    return 1;
}
