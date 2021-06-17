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
 * Author:  Eugene Vasilchenko
 *
 * File Description:
 *   Randomly test safeguards that must prevent some bad code variants from
 *   even compiling.
 *
 */


//#define NCBI_TEST_APPLICATION
#include <ncbi_pch.hpp>
#include <corelib/ncbicfg.h>
#include <corelib/error_codes.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <ncbi_random_macro.h>

#if !(defined(NCBI_STRICT_CTYPE_ARGS) && (defined(isalpha) || defined(NCBI_STRICT_CTYPE_ARGS_ACTIVE)))

#include <cctype>

#define NCBI_DEFINE_CTYPE_FUNC(ncbi_name, name)                         \
    inline int name(Uchar c) { return name(int(c)); }                   \
    inline int name(char c) { return name(Uchar(c)); }                  \
    template<class C> inline int name(C c)                              \
    { return See_the_standard_on_proper_argument_type_for_ctype_functions(c); }

NCBI_DEFINE_CTYPE_FUNC(NCBI_isalpha, isalpha)
NCBI_DEFINE_CTYPE_FUNC(NCBI_isalnum, isalnum)
NCBI_DEFINE_CTYPE_FUNC(NCBI_iscntrl, iscntrl)
NCBI_DEFINE_CTYPE_FUNC(NCBI_isdigit, isdigit)
NCBI_DEFINE_CTYPE_FUNC(NCBI_isgraph, isgraph)
NCBI_DEFINE_CTYPE_FUNC(NCBI_islower, islower)
NCBI_DEFINE_CTYPE_FUNC(NCBI_isprint, isprint)
NCBI_DEFINE_CTYPE_FUNC(NCBI_ispunct, ispunct)
NCBI_DEFINE_CTYPE_FUNC(NCBI_isspace, isspace)
NCBI_DEFINE_CTYPE_FUNC(NCBI_isupper, isupper)
NCBI_DEFINE_CTYPE_FUNC(NCBI_isxdigit, isxdigit)
NCBI_DEFINE_CTYPE_FUNC(NCBI_tolower, tolower)
NCBI_DEFINE_CTYPE_FUNC(NCBI_toupper, toupper)
//NCBI_DEFINE_CTYPE_FUNC(NCBI_isblank, isblank)
//NCBI_DEFINE_CTYPE_FUNC(NCBI_isascii, isascii)
//NCBI_DEFINE_CTYPE_FUNC(NCBI_toascii, toascii)

#undef NCBI_DEFINE_CTYPE_FUNC

#endif


#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;

class CTestApplication : public CNcbiApplication
{
public:
    virtual int Run(void);
};


int CTestApplication::Run(void)
{
#if defined(NCBI_RANDOM_VALUE_1)

#  if defined(NCBI_STRICT_GI)
#    define NCBI_N_TESTS 6
#  else
#    define NCBI_N_TESTS 5
#  endif

#  define NCBI_RANDOM_SLICE (NCBI_RANDOM_VALUE_MAX - NCBI_RANDOM_VALUE_MIN) / NCBI_N_TESTS
#  define NCBI_RANDOM_BOUNDARY(n)  (NCBI_RANDOM_VALUE_MIN + n * NCBI_RANDOM_SLICE)

#  if   NCBI_RANDOM_VALUE_1 < NCBI_RANDOM_BOUNDARY(1)
    _ASSERT(toupper('A' == 'A'));

#  elif NCBI_RANDOM_VALUE_1 < NCBI_RANDOM_BOUNDARY(2)
    toupper('A' == 'A');

#  elif NCBI_RANDOM_VALUE_1 < NCBI_RANDOM_BOUNDARY(3)
    toupper(1.);

#  elif NCBI_RANDOM_VALUE_1 < NCBI_RANDOM_BOUNDARY(4)
    toupper("string");

#  elif NCBI_RANDOM_VALUE_1 < NCBI_RANDOM_BOUNDARY(5)
    toupper(nullptr);

#  elif defined(NCBI_STRICT_GI)
    Int4 int4_gi = 123;
    TGi gi = int4_gi;  // must fail in the "Strict GI" mode!

#  elif NCBI_RANDOM_BOUNDARY(5) <= NCBI_RANDOM_VALUE_1  &&  NCBI_RANDOM_VALUE_1 <= NCBI_RANDOM_VALUE_MAX
    toupper("catch-all");  // catch the reminder
#  endif

    NcbiCerr << "Passed (BUT IT SHOULD NOT HAVE EVEN COMPILED. BAD!!!) "
             << NcbiEndl
             << "NCBI_RANDOM_VALUE_1 = " << NCBI_RANDOM_VALUE_1
             << NcbiEndl;
#else

    NcbiCerr << "NCBI_RANDOM_VALUE_1 is undefined" << NcbiEndl;

#endif  /* defined(NCBI_RANDOM_VALUE_1) */

    return 1;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[]) 
{
    return CTestApplication().AppMain(argc, argv);
}
