#ifndef ALGO_ALIGN_UTIL___GENOMIC_COMPART__HPP
#define ALGO_ALIGN_UTIL___GENOMIC_COMPART__HPP

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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <corelib/ncbistd.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


enum ECompartOptions {
    fCompart_AllowIntersections = 0x01,
    fCompart_SortByScore = 0x02,
    fCompart_SortByPctIdent = 0x04,
    fCompart_FilterByDiffLen = 0x08,

    fCompart_Defaults = 0
};
typedef int TCompartOptions;

void NCBI_XALGOALIGN_EXPORT 
FindCompartments(const list< CRef<CSeq_align> >& aligns,
                 list< CRef<CSeq_align_set> >& align_sets,
                 TCompartOptions options = fCompart_Defaults,
                 float diff_len_filter=3.0f);

// Join compartment, which is represented by Seq-align-set into
// one or more disc Seq-aligns which is suitable for nice graphical
// representation.
// It relies on a compartment structure - that is that alignments in
// the align set are sorted along query. It breaks the set into several
// alignments by gaps, longer than the gap_ratio*total_alignment_length
void NCBI_XALGOALIGN_EXPORT
JoinCompartment(const CRef<CSeq_align_set>& compartment,
                float gap_ratio,
                list< CRef<CSeq_align> >& aligns);


END_SCOPE(objects)
END_NCBI_SCOPE


#endif  // ALGO_ALIGN_UTIL___GENOMIC_COMPART__HPP
