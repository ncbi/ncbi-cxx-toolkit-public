#ifndef UTIL___BITSET_UTIL__HPP
#define UTIL___BITSET_UTIL__HPP

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
 * Authors:  Anatoliy Kuznetsov
 *
 */

/// @file ncbi_bitset_util.hpp
/// Bitset relates utilities

#include <util/bitset/ncbi_bitset.hpp>
#include <util/bitset/bmserial.h>

#include <vector>

BEGIN_NCBI_SCOPE


/// Serialize bitset into a vector<char> buffer
///
template<class TBV>
void BV_Serialize(const TBV&                   bv, 
                  vector<unsigned char>&       buf,
                  bm::word_t*                  tmp_block = 0)
{
    typename TBV::statistics st;
    bv.calc_stat(&st);

    if (st.max_serialize_mem > buf.size()) {
        buf.resize(st.max_serialize_mem);
    }
    size_t size = bm::serialize(bv, &buf[0], tmp_block);
    buf.resize(size);
}


END_NCBI_SCOPE

/*
* ===========================================================================
* $Log$
* Revision 1.3  2006/12/14 19:05:25  kuznets
* fixed typo
*
* Revision 1.2  2006/12/14 19:00:14  kuznets
* fixed typo
*
* Revision 1.1  2006/12/13 20:25:19  kuznets
* initial revision
*
*
* ===========================================================================
*/

#endif /* UTIL___BITSET_UTIL__HPP */
