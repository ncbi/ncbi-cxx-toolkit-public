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
 * Author:  Anatoliy Kuznetsov
 *
 * File Description: BM library (bitsets) test
 *   
 */   

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <util/bitset/ncbi_bitset.hpp>

#include <test/test_assert.h>

USING_NCBI_SCOPE;

static
void s_TEST_SetBit()
{
    bm::bvector<>   bv;    // Bitvector variable declaration.

    unsigned cnt = bv.count();
    assert(cnt == 0);

    // Set some bits.

    bv.set(10);
    bv.set(100);
    bv.set(1000000);

    // New bitvector's count.
    cnt = bv.count();
    assert(cnt == 3);

    // Print the bitvector.

    unsigned value = bv.get_first();
    do
    {
        value = bv.get_next(value);
        if (value)
        {
            assert(value == 10 || value == 100 || value == 1000000);
        }
        else
        {
            break;
        }
    } while(1);

    cout << endl;

    bv.clear();   // Clean up.
    
    cnt = bv.count();
    assert(cnt == 0);

    // We also can use operators to set-clear bits;

    bv[10] = true;
    bv[100] = true;
    bv[10000] = true;

    cnt = bv.count();
    assert(cnt == 3);

    if (bv[10])
    {
        bv[10] = false;
    }
    
    cnt = bv.count();
    assert(cnt == 2);
}

int main(int, const char**)
{
    s_TEST_SetBit();
    return 0;
}


/*
* ===========================================================================
*
* $Log$
* Revision 1.2  2004/05/17 21:06:57  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.1  2004/04/14 14:47:29  kuznets
* Initial revision
*
*
* ===========================================================================
*/

