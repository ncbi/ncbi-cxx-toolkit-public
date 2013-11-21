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

#ifndef OBJTOOLS_WRITERS___GFF3_SPLICEDSEG_RECORD__HPP
#define OBJTOOLS_WRITERS___GFF3_SPLICEDSEG_RECORD__HPP

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objtools/alnmgr/alnmap.hpp>
#include <objtools/writers/gff2_write_data.hpp>
#include <objtools/writers/gff3_alignment_data.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
class NCBI_XOBJWRITE_EXPORT CGffSplicedSegRecord
//  ----------------------------------------------------------------------------
    : public CGffAlignmentRecord
{
public:
    CGffSplicedSegRecord(
            unsigned int uFlags =0,
            unsigned int uRecordId =0):
        CGffAlignmentRecord(uFlags, uRecordId)
    {
    };

    virtual ~CGffSplicedSegRecord() {};

public:
    bool Initialize(
        CScope&,
        const CSeq_align&,
        const CSpliced_exon&);

protected:
    bool xSetId(
        CScope&,
        const CSeq_align&,
        const CSpliced_exon&);

    bool xSetMethod(
        CScope&,
        const CSeq_align&,
        const CSpliced_exon&);

    bool xSetType(
        CScope&,
        const CSeq_align&,
        const CSpliced_exon&);

    bool xSetLocation(
        CScope&,
        const CSeq_align&,
        const CSpliced_exon&);

    bool xSetPhase(
        CScope&,
        const CSeq_align&,
        const CSpliced_exon&);

    bool xSetAttributes(
        CScope&,
        const CSeq_align&,
        const CSpliced_exon&);

    bool xSetScores(
        CScope&,
        const CSeq_align&,
        const CSpliced_exon&);

    bool xSetAlignment(
        CScope&,
        const CSeq_align&,
        const CSpliced_exon&);
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJTOOLS_WRITERS___GFF3_SPLICEDSEG_RECORD__HPP
