/*
*
*
* RCS Modification History:
* $Log$
* Revision 1.2  1995/05/17 17:57:23  epstein
* add RCS log revision history
*
*/

#include <s_types.h>

// #define is68k          // MPW C defines MC68000, mc68000, m68k, and
// #define isMACINTOSH    // macintosh automatically.

#ifndef MPW3              // It should define MPW3, but it doesn't
#define MPW3 1
#endif

// The endian stuff shouldn't really be here.
#ifndef BIG_ENDIAN
# define BIG_ENDIAN 4321
#endif

#ifndef LITTLE_ENDIAN
# define LITTLE_ENDIAN 1234
#endif

#ifndef BYTE_ORDER
# define BYTE_ORDER BIG_ENDIAN
#endif

#define MAXHOSTNAMELEN  256
#define MAXPATHLEN      256
#define NCARGS  0x100000  
