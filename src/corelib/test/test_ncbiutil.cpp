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
 * Author:  Aaron Ucko <ucko@ncbi.nlm.nih.gov>
 *
 * File Description:
 *   Test for the functionality in ncbiutil.hpp
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiutil.hpp>
#include <set>
#include <algorithm>

#include <test/test_assert.h>  /* This header must go last */

BEGIN_NCBI_SCOPE


static void TestPEqualTo(void)
{
    int             n1 = 17, n2 = 0, n3 = 17;
    set<const int*> s;

    s.insert(&n1);
    assert(find_if(s.begin(), s.end(), bind2nd(p_equal_to<int>(), &n2))
            == s.end());
    assert(find_if(s.begin(), s.end(), bind2nd(p_equal_to<int>(), &n3))
            != s.end());
    NcbiCout << "p_equal_to works." << NcbiEndl;
}

// Should add more tests here!


END_NCBI_SCOPE

USING_NCBI_SCOPE;


int main(int, char **) {
    TestPEqualTo();
    return 0;
}


/*
 * ===========================================================================
 * $Log$
 * Revision 6.4  2004/05/14 13:59:51  gorelenk
 * Added include of ncbi_pch.hpp
 *
 * Revision 6.3  2002/04/16 18:49:09  ivanov
 * Centralize threatment of assert() in tests.
 * Added #include <test/test_assert.h>. CVS log moved to end of file.
 *
 * Revision 6.2  2001/11/13 15:41:53  ucko
 * Tweak TestPEqualTo to compile on ICC.
 *
 * Revision 6.1  2001/11/09 20:05:05  ucko
 * Add (partial) test for ncbiutil
 *
 * ===========================================================================
 */
