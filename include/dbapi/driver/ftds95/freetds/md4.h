#ifndef MD4_H
#define MD4_H

/* $Id$ */

#include <freetds/pushvis.h>

/* Rename functions in order to avoid conflicts with other versions. */
#define MD4Init      FTDS95_MD4Init
#define MD4Update    FTDS95_MD4Update
#define MD4Transform FTDS95_MD4Transform
#define MD4Final     FTDS95_MD4Final

struct MD4Context
{
	TDS_UINT buf[4];
	TDS_UINT8 bytes;
	unsigned char in[64];
};

void MD4Init(struct MD4Context *context);
void MD4Update(struct MD4Context *context, unsigned char const *buf, size_t len);
void MD4Final(struct MD4Context *context, unsigned char *digest);
void MD4Transform(TDS_UINT buf[4], TDS_UINT const in[16]);

typedef struct MD4Context MD4_CTX;

#include <freetds/popvis.h>

#endif /* !MD4_H */
