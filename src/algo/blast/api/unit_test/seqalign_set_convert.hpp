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
 * Author: Christiam Camacho
 *
 */

/** @file seqalign_set_convert.hpp
 * Converts a Seq-align-set into a neutral seqalign for use with the
 * CSeqAlignCmp class
 */

#ifndef _SEQALIGN_SET_CONVERT_HPP
#define _SEQALIGN_SET_CONVERT_HPP

#include "neutral_seqalign.hpp"

BEGIN_SCOPE(ncbi)

// Forward declarations
BEGIN_SCOPE(objects)
    class CSeq_align_set;
END_SCOPE(objects)

BEGIN_SCOPE(blast)
BEGIN_SCOPE(qa)

void SeqAlignSetConvert(const objects::CSeq_align_set& ss,
                        std::vector<SeqAlign>& retval);

END_SCOPE(qa)
END_SCOPE(blast)
END_SCOPE(ncbi)

#endif /* _SEQALIGN_SET_CONVERT_HPP */
