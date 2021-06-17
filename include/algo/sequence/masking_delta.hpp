#ifndef ALGO_SEQUENCE___MASKING_DELTA__HPP
#define ALGO_SEQUENCE___MASKING_DELTA__HPP

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
 * Authors:  Eugene Vasilchenko
 *
 * File Description:
 *
 */

#include <corelib/ncbiobj.hpp>
#include <util/range.hpp>
#include <vector>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)

class CBioseq;
class CSeq_id;
class CSeq_loc;
class CSeq_id_Handle;

END_SCOPE(objects)

// Create virtual Bioseq for masking original sequence with gaps.
// New sequence will has Seq-id 'new_id'
// Its Seq-inst object will be of type delta, and has reference to
// the original sequence ('original_id') and gaps in place of masked ranges.
CRef<objects::CBioseq> NCBI_XALGOSEQ_EXPORT
MakeMaskingBioseq(const objects::CSeq_id& new_id,
                  TSeqPos seq_length,
                  const objects::CSeq_id& original_id,
                  const vector<CRange<TSeqPos> >& mask_ranges);

// Create virtual Bioseq for masking original sequence with gaps.
// New sequence will has Seq-id 'new_id'
// Its Seq-inst object will be of type delta, and has reference to
// the original sequence ('original_id') and gaps in place of masked ranges.
// The masked ranges are specified by 'masking_loc'.
CRef<objects::CBioseq> NCBI_XALGOSEQ_EXPORT
MakeMaskingBioseq(const objects::CSeq_id& new_id,
                  TSeqPos seq_length,
                  const objects::CSeq_id& original_id,
                  const objects::CSeq_loc& masking_loc);

// Create virtual Bioseq for masking original sequence with gaps.
// New sequence will has Seq-id 'new_id'
// Its Seq-inst object will be of type delta, and has reference to
// the original sequence gaps in place of masked ranges.
// The original sequence and masked ranges are specified by 'masking_loc'.
CRef<objects::CBioseq> NCBI_XALGOSEQ_EXPORT
MakeMaskingBioseq(const objects::CSeq_id& new_id,
                  TSeqPos seq_length,
                  const objects::CSeq_loc& masking_loc);


END_NCBI_SCOPE

#endif // ALGO_SEQUENCE___MASKING_DELTA__HPP
