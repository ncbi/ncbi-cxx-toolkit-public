#ifndef OBJTOOLS_READERS___CIGAR__HPP
#define OBJTOOLS_READERS___CIGAR__HPP

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
 * Authors:  Aaron Ucko
 *
 */

/// @file cigar.hpp
/// Code to handle Concise Idiosyncratic Gapped Alignment Report notation.
///
/// See http://www.ensembl.org/Docs/wiki/html/EnsemblDocs/CigarFormat.html
/// for the base format and http://song.sorceforge.net/gff3-jan04.shtml for
/// the frame-shift extensions.

#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqloc/Seq_loc.hpp>

/** @addtogroup Miscellaneous
 *
 * @{
 */


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_interval;

struct SCigarAlignment
{
    enum EOperation {
        eNotSet       = 0,   ///< for internal use only
        eMatch        = 'M',
        eInsertion    = 'I',
        eDeletion     = 'D',
        eIntron       = 'N', ///< not documented by GFF3
        eForwardShift = 'F', ///< only documented by GFF3 (same for R)
        eReverseShift = 'R'
    };

    struct SSegment
    {
        EOperation op;
        TSeqPos    len;
    };
    typedef vector<SSegment> TSegments;

    SCigarAlignment(const string& s);
    CRef<CSeq_align> operator()(const CSeq_interval& ref,
                                const CSeq_interval& tgt);

    TSegments segments;

private:
    void x_AddAndClear(SSegment& seg)
        { segments.push_back(seg);  seg.op = eNotSet;  seg.len = 0; }

    CRef<CSeq_loc> x_NextChunk(const CSeq_id& id, TSeqPos pos,
                               TSignedSeqPos len);
};

END_SCOPE(objects)
END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2004/06/07 20:42:35  ucko
 * Add a reader for CIGAR alignments, as used by GFF 3.
 *
 * ===========================================================================
 */

#endif  /* OBJTOOLS_READERS___CIGAR__HPP */
