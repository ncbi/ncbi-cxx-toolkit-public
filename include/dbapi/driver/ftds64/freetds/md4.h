#ifndef MD4_H
#define MD4_H

/* $Id$ */

/* Redefine function's names in order to avoid conflicts with libmd.so in Solaris. */
#define MD4Init      FTDS64_MD4Init
#define MD4Update    FTDS64_MD4Update
#define MD4Transform FTDS64_MD4Transform
#define MD4Final     FTDS64_MD4Final


struct MD4Context
{
	TDS_UINT buf[4];
	TDS_UINT bits[2];
    union {
        unsigned char in[64];
        TDS_UINT in_uints[16];
    } u;
};

void MD4Init(struct MD4Context *context);
void MD4Update(struct MD4Context *context, unsigned char const *buf, size_t len);
void MD4Final(struct MD4Context *context, unsigned char *digest);
void MD4Transform(TDS_UINT buf[4], TDS_UINT const in[16]);

typedef struct MD4Context MD4_CTX;

#endif /* !MD4_H */
