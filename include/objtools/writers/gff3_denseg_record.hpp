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
 * Author: Frank Ludwig
 *
 * File Description:
 *   GFF3 transient data structures
 *
 */

#ifndef OBJTOOLS_WRITERS___GFF3DENSEG_RECORD__HPP
#define OBJTOOLS_WRITERS___GFF3DENSEG_RECORD__HPP

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objtools/alnmgr/alnmap.hpp>
#include <objtools/writers/gff3_alignment_data.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
class NCBI_XOBJWRITE_EXPORT CGffDenseSegRecord
//  ----------------------------------------------------------------------------
    : public CGffAlignmentRecord
{
public:
    /// Initialize GFF3 output record, to be filled later from a Dense-seg
    /// @param uFlags
    ///   mode flags of the writer creating this record.
    /// @param uRecordId
    ///   record ID, used to set the GFF3 ID attribute. 
    ///
    CGffDenseSegRecord(
            unsigned int uFlags =0,
            unsigned int uRecordId =0):
        CGffAlignmentRecord(uFlags, uRecordId)
    {};

    virtual ~CGffDenseSegRecord() {};

public:
    /// Initialize all GFF3 record fields using the provided information
    /// @param scope
    ///   scope object used for seq-id lookup and conversion.
    /// @param align
    ///   container alignment object of the given Dense-seg.
    /// @param alnMap
    ///   alignment map created from the given Dense-seg.
    /// @param row
    ///   row in the alignment map this record should be constructed from.
    bool Initialize(
        CScope& scope,
        const CSeq_align& align,
        const CAlnMap& alnMap,
        unsigned int row);

protected:
    bool xInitAlignmentIds(
        CScope&,
        const CSeq_align&,
        const CAlnMap&,
        unsigned int row);

    bool xSetId(
        CScope&,
        const CSeq_align&,
        const CAlnMap&,
        unsigned int row);

    bool xSetMethod(
        CScope&,
        const CSeq_align&,
        const CAlnMap&,
        unsigned int row);
    
    bool xSetType(
        CScope&,
        const CSeq_align&,
        const CAlnMap&,
        unsigned int row);

    bool xSetAlignment(
        CScope&,
        const CSeq_align&,
        const CAlnMap&,
        unsigned int row);

    bool xSetScores(
        CScope&,
        const CSeq_align&,
        const CAlnMap&,
        unsigned int row);

    bool xSetAttributes(
        CScope&,
        const CSeq_align&,
        const CAlnMap&,
        unsigned int row);

    void xAddInsertion(
        const CAlnMap::TSignedRange& targetPiece ); 

    void xAddDeletion(
        const CAlnMap::TSignedRange& sourcePiece ); 

    void xAddMatch(
        const CAlnMap::TSignedRange& sourcePiece,
        const CAlnMap::TSignedRange& targetPiece ); 

    CConstRef<CSeq_id> mpSourceId;
    CConstRef<CSeq_id> mpTargetId;
    CAlnMap::TSignedRange mTargetRange;
    CAlnMap::TSignedRange mSourceRange;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJTOOLS_WRITERS___GFF3DENSEG_RECORD__HPP
