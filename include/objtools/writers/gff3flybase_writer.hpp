#ifndef ___GFF3_FLYBASE_WRITER__HPP
#define ___GFF3_FLYBASE_WRITER__HPP
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
 * Authors:  Eyal Mozes
 *
 * File Description:
 */
 
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Spliced_exon.hpp>
#include <objtools/writers/gff3_writer.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

//  ---------------------------------------------------------------------------- 
class NCBI_XOBJWRITE_EXPORT CGff3FlybaseWriter 
    : public objects::CGff3Writer {
//  ----------------------------------------------------------------------------
public:
    CGff3FlybaseWriter(
        objects::CScope& scope,
        CNcbiOstream& ostr)
    : objects::CGff3Writer(scope, ostr)
    {}

    virtual bool WriteHeader();

protected:
    virtual bool xWriteAlignDisc(
        const CSeq_align&,
        const string& = "") override;

    virtual bool xAssignAlignmentSplicedLocation(
        CGffAlignRecord&,
        const CSpliced_seg&,
        const CSpliced_exon&) override;

    virtual bool xAssignAlignmentSplicedTarget(
        CGffAlignRecord&,
        const CSpliced_seg&,
        const CSpliced_exon&) override;

    virtual bool xAssignAlignmentSplicedSeqId(
        CGffAlignRecord&,
        const CSpliced_seg&,
        const CSpliced_exon&) override;

    virtual bool xAssignAlignmentDensegSeqId(
        CGffAlignRecord&,
        const CAlnMap&,
        unsigned int) override;

    virtual bool xAssignAlignmentDensegTarget(
        CGffAlignRecord&,
        const CAlnMap&,
        unsigned int) override;

    virtual bool xAssignAlignmentDensegLocation(
        CGffAlignRecord&,
        const CAlnMap&,
        unsigned int) override;

    virtual bool xAssignAlignmentScores(
        CGffAlignRecord&,
        const CSeq_align&) override;

    bool xAssignTaxid(
        CBioseq_Handle,
        CGffAlignRecord&);

    bool xAssignDefline(
        CBioseq_Handle,
        CGffAlignRecord&);

    map<string, string> mTaxidMap;
    map<string, string> mDeflineMap;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif  // ___GFF3_FLYBASE_WRITER__HPP
