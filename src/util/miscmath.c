/*  $Id$
* ===========================================================================
*
* Much of this code comes from the freely distributable math library
* fdlibm, for which the following notice applies:
*
* Copyright (C) 1993-2004 by Sun Microsystems, Inc. All rights reserved.
*
* Developed at SunSoft, a Sun Microsystems, Inc. business.
* Permission to use, copy, modify, and distribute this
* software is freely granted, provided that this notice 
* is preserved.
*
* ===========================================================================
*
* Author (editor, really):  Aaron Ucko, NCBI
*
* File Description:
*   Miscellaneous math functions that might not be part of the
*   system's standard libraries.
*
* ===========================================================================
*/

#include <ncbiconf.h>
#include <util/miscmath.h>

/* erf/erfc implementation, cloned into algo/blast/core. */
#define NCBI_INCLUDE_NCBI_ERF_C 1
#include "ncbi_erf.c"
