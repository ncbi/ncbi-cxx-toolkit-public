#ifndef CORELIB___NCBICTYPE__HPP
#define CORELIB___NCBICTYPE__HPP

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
 * Author:  Denis Vakatov, Eugene Vasilchenko
 *
 *
 */

/// @file ncbictype.hpp
/// Catch attempts to call ctype functions with bad types


#include <corelib/ncbistl.hpp>
#include <corelib/ncbitype.h>
#include <cctype>

#if !defined(NCBI_NO_STRICT_CTYPE_ARGS)
#  define NCBI_STRICT_CTYPE_ARGS
#endif

#ifdef NCBI_STRICT_CTYPE_ARGS

# define NCBI_STRICT_CTYPE_ARGS_ACTIVE

BEGIN_STD_NAMESPACE;

#define NCBI_DEFINE_CTYPE_FUNC(name)                    \
    inline int name(Uchar c) { return name(int(c)); }   \
    inline int name(char c) { return name(Uchar(c)); }  \
    template<class C> inline int name(C c)              \
    { \
        return See_the_standard_on_proper_argument_type_for_ctype_functions(c); \
        return 0; /* to avoid a compilation warning */ \
    } 

NCBI_DEFINE_CTYPE_FUNC(isalpha)
NCBI_DEFINE_CTYPE_FUNC(isalnum)
NCBI_DEFINE_CTYPE_FUNC(iscntrl)
NCBI_DEFINE_CTYPE_FUNC(isdigit)
NCBI_DEFINE_CTYPE_FUNC(isgraph)
NCBI_DEFINE_CTYPE_FUNC(islower)
NCBI_DEFINE_CTYPE_FUNC(isprint)
NCBI_DEFINE_CTYPE_FUNC(ispunct)
NCBI_DEFINE_CTYPE_FUNC(isspace)
NCBI_DEFINE_CTYPE_FUNC(isupper)
NCBI_DEFINE_CTYPE_FUNC(isxdigit)
NCBI_DEFINE_CTYPE_FUNC(tolower)
NCBI_DEFINE_CTYPE_FUNC(toupper)
//NCBI_DEFINE_CTYPE_FUNC(isblank)
//NCBI_DEFINE_CTYPE_FUNC(isascii)
//NCBI_DEFINE_CTYPE_FUNC(toascii)

#undef NCBI_DEFINE_CTYPE_FUNC

END_STD_NAMESPACE;

#endif // NCBI_STRICT_CTYPE_ARGS

#endif /* CORELIB___NCBICTYPE__HPP */
