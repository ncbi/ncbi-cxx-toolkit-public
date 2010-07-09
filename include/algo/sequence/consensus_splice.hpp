#ifndef ALGO_SEQ__CONSENSUS_SPLICE__HPP
#define ALGO_SEQ__CONSENSUS_SPLICE__HPP

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
 * Authors:  Josh Cherry
 *
 * File Description: Simple functions for determining whether a pair of
 *    dinucleotides form a consensus splice sequence
 *
 */

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE


/// Consensus splice is GY..AG or AT..AC
inline bool IsConsensusSplice(const string& splice5,
                              const string& splice3)
{
    return (NStr::EqualNocase(splice3, "AG") &&
            (NStr::EqualNocase(splice5, "GT") ||
             NStr::EqualNocase(splice5, "GC")))  ||
           (NStr::EqualNocase(splice5, "AT") &&
            NStr::EqualNocase(splice3, "AC"));
}


/// Strict consensus splice is GT..AG
inline bool IsStrictConsensusSplice(const string& splice5,
                                    const string& splice3)
{
    return NStr::EqualNocase(splice5, "GT")  &&
           NStr::EqualNocase(splice3, "AG");
}


END_NCBI_SCOPE

#endif  // ALGO_SEQ__CONSENSUS_SPLICE__HPP
