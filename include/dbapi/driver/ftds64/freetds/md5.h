#ifndef MD5_H
#define MD5_H

/* $Id$ */

/* Redefine function's names in order to avoid conflicts with libmd5.so in Solaris. */
#define MD5Init      FTDS64_MD5Init
#define MD5Update    FTDS64_MD5Update
#define MD5Transform FTDS64_MD5Transform
#define MD5Final     FTDS64_MD5Final


struct MD5Context {
	TDS_UINT buf[4];
	TDS_UINT bits[2];
    union {
        unsigned char in[64];
        TDS_UINT in_uints[16];
    } u;
};

void MD5Init(struct MD5Context *context);
void MD5Update(struct MD5Context *context, unsigned char const *buf,
               size_t len);
void MD5Final(struct MD5Context *context, unsigned char *digest);
void MD5Transform(TDS_UINT buf[4], TDS_UINT const in[16]);

/*
 * This is needed to make RSAREF happy on some MS-DOS compilers.
 */
typedef struct MD5Context MD5_CTX;

#endif /* !MD5_H */
