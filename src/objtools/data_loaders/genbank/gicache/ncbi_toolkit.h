#include <corelib/ncbi_limits.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#ifndef Boolean
typedef unsigned char	Nlm_Boolean;
#define Boolean		Nlm_Boolean
#endif

#ifndef TRUE
#define TRUE ((Nlm_Boolean)1)
#endif

#ifndef FALSE
#define FALSE ((Nlm_Boolean)0)
#endif

#ifndef NULLB
#define NULLB '\0'
#endif

#ifndef INLINE
#ifdef __cplusplus
#define INLINE inline
#else
#define INLINE
#endif
#endif
