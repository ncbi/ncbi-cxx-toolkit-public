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
 * Author:  Aaron Ucko
 *
 * File Description:
 *   Test of CConstResizingIterator.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <util/resize_iter.hpp>

#include <test/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;

int main(int argc, char** argv) {
    if (argc != 2) {
        NcbiCerr << "Usage: test_resize_iter <string>" << endl;
        return 1;
    }
    
    cout << hex;
    string s(argv[1]);
    for (unsigned int new_size = 1;  new_size <= 32;  new_size++) {
        for (unsigned int count = 1; count <= 2; count++) {
            CConstResizingIterator<string> it(s, new_size);
            string s2(s.size(), '?');
            CResizingIterator<string> it2(s2, new_size);
            for (unsigned int n = 0;  n < s.size() * CHAR_BIT / new_size;
                 n += count) {
                for (unsigned int i = 0; i < count; ++i) {
                    int value = *it;
                    *it2 = value; // ignore bogus WorkShop complaints here
                    ++it2;
                    cout << value;
                    if (new_size > 4)
                        cout << ' ';
                }
                for (unsigned int i = 0; i < count; ++i) {
                    ++it;
                }
            }
            cout << ' ' << NStr::PrintableString(s2) << ' ' << it.AtEnd()
                 << ' ' << it2.AtEnd() << endl;
        }
    }
    return 0;
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.6  2004/05/17 21:09:26  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.5  2004/02/12 20:11:48  ucko
 * Use NStr::PrintableString, since we may get non-ASCII characters.
 *
 * Revision 1.4  2002/12/30 20:56:51  ucko
 * Note location of bogus WorkShop warnings.
 *
 * Revision 1.3  2002/12/30 20:39:11  ucko
 * kBitsPerByte changed to CHAR_BIT; use preincrement rather than post-.
 *
 * Revision 1.2  2002/04/16 18:52:16  ivanov
 * Centralize threatment of assert() in tests.
 * Added #include <test/test_assert.h>. CVS log moved to end of file.
 *
 * Revision 1.1  2001/09/04 14:06:32  ucko
 * Add resizing iterators for sequences whose representation uses an
 * unnatural unit size -- for instance, ASN.1 octet strings corresponding
 * to sequences of 32-bit integers or of packed nucleotides.
 *
 * ===========================================================================
*/

