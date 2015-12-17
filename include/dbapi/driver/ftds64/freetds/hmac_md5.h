#ifndef HMAC_MD5_H
#define HMAC_MD5_H

/* $Id$ */

#include "md5.h"

typedef struct
{
    MD5_CTX         ctx;
    unsigned char   k_ipad[65];
    unsigned char   k_opad[65];
} HMACMD5Context;

void hmac_md5_init_limK_to_64(const unsigned char* key,
                              size_t key_len,
                              HMACMD5Context *ctx);
void hmac_md5_update(const unsigned char* text,
                     size_t text_len,
                     HMACMD5Context *ctx);
void hmac_md5_final(unsigned char* digest,
                    HMACMD5Context *ctx);
void hmac_md5(const unsigned char key[16],
              const unsigned char* data,
              size_t data_len,
              unsigned char* digest);
void hmac_md5_init_rfc2104(const unsigned char* key,
                           size_t key_len,
                           HMACMD5Context *ctx);

#endif /* !HMAC_MD5_H */
