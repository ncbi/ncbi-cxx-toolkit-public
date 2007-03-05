/*  $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
* Author:  Aaron Ucko, Sergey Sikorskiy
*
* File Description:
*   Meta-source file including replacements for system functions
*   lacking on some platforms.
*
* ===========================================================================
*/

#include <ncbiconf.h>

#ifdef software_version
#undef software_version
#endif

#ifdef no_unused_var_warn
#undef no_unused_var_warn
#endif

#ifndef HAVE_ASPRINTF
#define software_version asprintf_software_version
#define no_unused_var_warn asprintf_no_unused_var_warn
#include "../replacements/asprintf.c"
#undef software_version
#undef no_unused_var_warn
#endif

#ifndef HAVE_ATOLL
#define software_version atoll_software_version
#define no_unused_var_warn atoll_no_unused_var_warn
#include "../replacements/atoll.c"
#undef software_version
#undef no_unused_var_warn
#endif

#ifndef HAVE_ICONV
#define software_version iconv_software_version
#define no_unused_var_warn iconv_no_unused_var_warn
#include "../replacements/iconv.c"
#undef software_version
#undef no_unused_var_warn
#endif

#ifndef HAVE_STRTOK_R
#define software_version strtok_r_software_version
#define no_unused_var_warn strtok_r_no_unused_var_warn
#include "../replacements/strtok_r.c"
#undef software_version
#undef no_unused_var_warn
#endif

#ifndef HAVE_VASPRINTF
#define software_version vasprintf_software_version
#define no_unused_var_warn vasprintf_no_unused_var_warn
#include "../replacements/vasprintf.c"
#undef software_version
#undef no_unused_var_warn
#endif

#ifndef HAVE_BASENAME
#define software_version basename_software_version
#define no_unused_var_warn basename_no_unused_var_warn
#include "../replacements/basename.c"
#undef software_version
#undef no_unused_var_warn
#endif

#ifndef HAVE_READPASSPHRASE
#define software_version readpassphrase_software_version
#define no_unused_var_warn readpassphrase_no_unused_var_warn
#include "../replacements/readpassphrase.c"
#undef software_version
#undef no_unused_var_warn
#endif

#ifndef HAVE_STRLCAT
#define software_version strlcat_software_version
#define no_unused_var_warn strlcat_no_unused_var_warn
#include "../replacements/strlcat.c"
#undef software_version
#undef no_unused_var_warn
#endif

#ifndef HAVE_STRLCPY
#define software_version strlcpy_software_version
#define no_unused_var_warn strlcpy_no_unused_var_warn
#include "../replacements/strlcpy.c"
#undef software_version
#undef no_unused_var_warn
#endif
