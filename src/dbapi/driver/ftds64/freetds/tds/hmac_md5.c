/* $Id$ */

#include "tds.h"
#include "hmac_md5.h"
#include <memory.h>

/***********************************************************************
 the rfc 2104 version of hmac_md5 initialisation.
***********************************************************************/
void hmac_md5_init_rfc2104(const unsigned char* key,
                           size_t key_len,
                           HMACMD5Context *ctx)
{
    int i;
    unsigned char tk[16];

    /* if key is longer than 64 bytes reset it to key=MD5(key) */
    if (key_len > 64) {
        struct MD5Context tctx;

        MD5Init(&tctx);
        MD5Update(&tctx, key, key_len);
        MD5Final(&tctx, tk);

        key = tk;
        key_len = 16;
    }

    /* start out by storing key in pads */
    memset(ctx->k_ipad, 0, sizeof(ctx->k_ipad));
    memset(ctx->k_opad, 0, sizeof(ctx->k_opad));

    memcpy( ctx->k_ipad, key, key_len);
    memcpy( ctx->k_opad, key, key_len);

    /* XOR key with ipad and opad values */
    for (i=0; i<64; i++) {
        ctx->k_ipad[i] ^= 0x36;
        ctx->k_opad[i] ^= 0x5c;
    }

    MD5Init(&ctx->ctx);
    MD5Update(&ctx->ctx, ctx->k_ipad, 64);
}

/***********************************************************************
 the microsoft version of hmac_md5 initialisation.
***********************************************************************/
void hmac_md5_init_limK_to_64(const unsigned char* key,
                              size_t key_len,
                              HMACMD5Context *ctx)
{
    /* if key is longer than 64 bytes truncate it */
    if (key_len > 64) {
        key_len = 64;
    }

    hmac_md5_init_rfc2104(key, key_len, ctx);
}

/***********************************************************************
 update hmac_md5 "inner" buffer
***********************************************************************/
void hmac_md5_update(const unsigned char* text,
                     size_t text_len,
                     HMACMD5Context *ctx)
{
    MD5Update(&ctx->ctx, text, text_len); /* then text of datagram */
}

/***********************************************************************
 finish off hmac_md5 "inner" buffer and generate outer one.
***********************************************************************/
void hmac_md5_final(unsigned char* digest, HMACMD5Context *ctx)
{
    struct MD5Context ctx_o;

    MD5Final(&ctx->ctx, digest);

    MD5Init(&ctx_o);
    MD5Update(&ctx_o, ctx->k_opad, 64);
    MD5Update(&ctx_o, digest, 16);
    MD5Final(&ctx_o, digest);
}

/***********************************************************
 single function to calculate an HMAC MD5 digest from data.
 use the microsoft hmacmd5 init method because the key is 16 bytes.
************************************************************/
void hmac_md5(const unsigned char key[16],
              const unsigned char* data,
              size_t data_len,
              unsigned char* digest)
{
    HMACMD5Context ctx;

    hmac_md5_init_limK_to_64(key, 16, &ctx);

    if (data_len != 0) {
        hmac_md5_update(data, data_len, &ctx);
    }

    hmac_md5_final(digest, &ctx);
}


