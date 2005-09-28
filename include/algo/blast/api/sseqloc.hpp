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
 * Author:  Christiam Camacho
 *
 */

/** @file sseqloc.hpp
 * Definition of SSeqLoc structure
 */

#ifndef ALGO_BLAST_API___SSEQLOC__HPP
#define ALGO_BLAST_API___SSEQLOC__HPP

#include <corelib/ncbistd.hpp>
#include <objmgr/scope.hpp>
#include <objects/seqloc/Seq_loc.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Structure combining a Seq-loc, scope and masking locations for one sequence
struct SSeqLoc {
    /// Seq-loc describing the sequence to use as query/subject to BLAST
    /// The types of Seq-loc currently supported are: whole, seq-interval...
    /// @todo FIXME Complete the list of supported seq-locs
    CConstRef<objects::CSeq_loc>     seqloc;

    /// Scope where the sequence referenced can be found by the toolkit's
    /// object manager
    mutable CRef<objects::CScope>    scope;

    /// Seq-loc describing regions to mask in the seqloc field
    CConstRef<objects::CSeq_loc>     mask;

    SSeqLoc()
        : seqloc(), scope(), mask() {}
    SSeqLoc(const objects::CSeq_loc* sl, objects::CScope* s)
        : seqloc(sl), scope(s), mask(0) {}
    SSeqLoc(const objects::CSeq_loc& sl, objects::CScope& s)
        : seqloc(&sl), scope(&s), mask(0) {}
    SSeqLoc(const objects::CSeq_loc* sl, objects::CScope* s,
            const objects::CSeq_loc* m)
        : seqloc(sl), scope(s), mask(m) {}
    SSeqLoc(const objects::CSeq_loc& sl, objects::CScope& s,
            const objects::CSeq_loc& m)
        : seqloc(&sl), scope(&s), mask(&m) {}
};

/// Vector of sequence locations
typedef vector<SSeqLoc>   TSeqLocVector;

END_SCOPE(blast)
END_NCBI_SCOPE

#endif  /* ALGO_BLAST_API___SSEQLOC__HPP */
