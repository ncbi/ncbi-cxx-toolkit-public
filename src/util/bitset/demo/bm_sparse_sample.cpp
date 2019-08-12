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
 * File Description: Example on how to construct sparse arrays with bvector<>
 *   
 */   

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <util/bitset/ncbi_bitset.hpp>


USING_NCBI_SCOPE;


// Bit-vector encoded sparse store principle:
//
//    0 0 1 0 0 0 1 0 0 0 0 ....  -  position encoding bit-vector
//
//    10, 35, ...                 -  sparse vector
//
// Every ON bit in the encoding bit-vector has a corresponding element in 
// sparse array. Thus bit count of bit vector is the same as number of elements
// in sparse vector.
// If bit N in bit vector is 1, index of corresponding element in sparse array is
// calculated as BITCOUNT(0, N-1)
//
//                *------------| Bit 7 is ON
//                !
//    0 0 1 0 0 0 1 0 0 0 0 ....  -  position encoding bit-vector
//    |         |
//    +---------+
//         BitCount(0, 6) = 1
//    Real index in sparse array for the element 7 is 1
//    Value = 35
//
int main(int, const char**)
{
    vector<int>    svect;
    bm::bvector<>  bv;

    // Init the bit-vector and sparse array.
    //
    bv[100]=true;
    svect.push_back(100);
    bv[10000]=true;
    svect.push_back(10000);
    bv[90000]=true;
    svect.push_back(90000);
    bv[90001]=true;
    svect.push_back(90001);


    // Random access to sparse array elements using count_range
    //
    if (bv[100]) {
        unsigned sparse_idx = bv.count_range(0, 100 - 1); 
        cout << "Element " << 100 << " " << svect[sparse_idx] << endl;
    }

    if (bv[90001]) {
        unsigned sparse_idx = bv.count_range(0, 90001 - 1); 
        cout << "Element " << 90001 << " " << svect[sparse_idx] << endl;
    }
    
    cout << endl;

    // Random access to sparse array elements using count_range
    // and precomputed skip list. Skip list gives faster random access.
    //

    bm::bvector<>::rs_index_type rs_index;
    bv.build_rs_index(&rs_index);

    if (bv[100]) {
        unsigned sparse_idx = bv.count_range(0, 100 - 1, rs_index); 
        cout << "Element " << 100 << " " << svect[sparse_idx] << endl;
    }

    if (bv[90001]) {
        unsigned sparse_idx = bv.count_range(0, 90001 - 1, rs_index); 
        cout << "Element " << 90001 << " " << svect[sparse_idx] << endl;
    }

    cout << endl;

    // Sequential access using counted enumerator
    //
    bm::bvector<>::counted_enumerator en = bv.first();
    for (; en.valid(); ++en) {
        unsigned bit_idx = *en;
        unsigned sparse_idx = en.count()-1;
        cout << "Element " << bit_idx << " " << svect[sparse_idx] << endl;
    }

    return 0;
}
