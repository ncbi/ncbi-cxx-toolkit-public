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
 * Author:  Denis Vakatov
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbi_limits.hpp>
#include <test/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;


template <class T>
void s_TestType(const char* type_name, T value)
{
    T minval = numeric_limits<T>::min(), maxval = numeric_limits<T>::max();

    LOG_POST("--------------");

    LOG_POST(type_name << ":  "
             << minval << " <= " << value  << " <= " << maxval);

    assert(minval <= value);
    assert(maxval >= value);
    assert(minval < maxval);

    if (value != (T) (int) value)
        return;

    T min_minus_1 = minval - (T) 1;
    LOG_POST(type_name << ":  "
             << minval << " <= " << min_minus_1  << " <= " << maxval);

    assert(minval <= min_minus_1);
    assert(maxval >= min_minus_1);

    T max_plus_1 = maxval + (T) 1;
    LOG_POST(type_name << ":  "
             << minval << " <= " << max_plus_1  << " <= " << maxval);

    assert(minval <= max_plus_1);
    assert(maxval >= max_plus_1);
}


#define TEST_TYPE(type_name, value) \
  { s_TestType(#type_name, (type_name) value); }

static void s_Test(void)
{
    TEST_TYPE(         char,   'a');
    TEST_TYPE(signed   char,   'a');
    TEST_TYPE(unsigned char,   'a');
    TEST_TYPE(signed   short,  -555);
    TEST_TYPE(unsigned short,  555);
    TEST_TYPE(signed   int,    -555);
    TEST_TYPE(unsigned int,    555);
    TEST_TYPE(signed   long,   -555);
    TEST_TYPE(unsigned long,   555);
    TEST_TYPE(float,   55.5);
    TEST_TYPE(double,  5.55);

#if (SIZEOF_LONG_LONG > 0) \
    && (!defined(__GLIBCPP__) || defined(_GLIBCPP_USE_LONG_LONG))
    TEST_TYPE(signed   long long,   -555);
    TEST_TYPE(unsigned long long,   555);
#endif

#if (SIZEOF___INT64 == 8)
    // This does not work as there is no "ostream::operator<<(const __int64&)"
    //    TEST_TYPE(signed   __int64, -555);
    //    TEST_TYPE(unsigned __int64, 555);
#endif
}



/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main()
{
    // Set err.-posting and tracing to maximum
    SetDiagTrace(eDT_Enable);
    SetDiagPostFlag(eDPF_All);
    SetDiagPostLevel(eDiag_Info);

    // Run tests
    try {
        s_Test();
    } catch (exception& e) {
        ERR_POST(Fatal << e.what());
    }

    // Success
    return 0;
}


/*
 * ===========================================================================
 * $Log$
 * Revision 6.5  2004/05/14 13:59:51  gorelenk
 * Added include of ncbi_pch.hpp
 *
 * Revision 6.4  2004/02/10 16:31:45  ucko
 * s_TestType: store the result of min() and max() in locals to work
 * around a platform-dependent GCC 2.95 bug.
 *
 * Revision 6.3  2002/04/16 18:49:07  ivanov
 * Centralize threatment of assert() in tests.
 * Added #include <test/test_assert.h>. CVS log moved to end of file.
 *
 * Revision 6.2  2001/09/19 17:55:17  ucko
 * Fix GCC 3 build.
 *
 * Revision 6.1  2001/01/03 17:40:31  vakatov
 * Initial revision
 *
 * ===========================================================================
 */
