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
 *   GVF file reader
 *
 */

#ifndef OBJTOOLS_READERS___GVF_READER__HPP
#define OBJTOOLS_READERS___GVF_READER__HPP

#include <corelib/ncbistd.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seqfeat/Seq_feat.hpp>

#include <objtools/readers/gff3_reader.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CGff3ReadRecord;
class CReaderListener;

//  ============================================================================
class CGvfReadRecord
//  ============================================================================
    : public CGff3ReadRecord
{
public:
    CGvfReadRecord(
        unsigned int lineNumber,
        ILineErrorListener* pEC = nullptr):
        mLineNumber(lineNumber),
        mpMessageListener(pEC)
    {};
    ~CGvfReadRecord() {};

    bool AssignFromGff(
        const string& ) override;

    bool SanityCheck() const;

protected:
    bool xAssignAttributesFromGff(
        const string&,
        const string& ) override;

    unsigned int mLineNumber;
    ILineErrorListener* mpMessageListener;
};

//  ----------------------------------------------------------------------------
class NCBI_XOBJREAD_EXPORT CGvfReader
//  ----------------------------------------------------------------------------
    : public CGff3Reader
{
public:
    CGvfReader(
        unsigned int uFlags,
        const string& name = "",
        const string& title = "",
        CReaderListener* = nullptr);

    virtual ~CGvfReader();

    CRef<CSeq_annot>
    ReadSeqAnnot(
        ILineReader&,
        ILineErrorListener* = nullptr ) override;

protected:
    void xGetData(
        ILineReader&,
        TReaderData&) override;

    void xProcessData(
        const TReaderData&,
        CSeq_annot&) override;

    bool xParseStructuredComment(
        const string&) override;

    bool xParseFeature(
        const string&,
        CSeq_annot&,
        ILineErrorListener*) override;

    void xPostProcessAnnot(
        CSeq_annot&) override;

    CRef<CSeq_annot> x_GetAnnotById(
        TAnnots& annots,
        const string& strId );

    virtual bool xMergeRecord(
        const CGvfReadRecord&,
        CSeq_annot&,
        ILineErrorListener*);

    bool xFeatureSetLocation(
        const CGff2Record&,
        CSeq_feat&);

    bool xFeatureSetLocationInterval(
        const CGff2Record&,
        CSeq_feat&);

    bool xFeatureSetLocationPoint(
        const CGff2Record&,
        CSeq_feat&);

    bool xSetLocation(
        const CGff2Record&,
        CSeq_loc&);

    bool xSetLocationInterval(
        const CGff2Record&,
        CSeq_loc&);

    bool xSetLocationPoint(
        const CGff2Record&,
        CSeq_loc&);

    bool xFeatureSetVariation(
        const CGvfReadRecord&,
        CSeq_feat&);

    virtual bool xFeatureSetExt(
        const CGvfReadRecord&,
        CSeq_feat&,
        ILineErrorListener*);

    bool xVariationMakeSNV(
        const CGvfReadRecord&,
         CVariation_ref&);

    bool xVariationMakeCNV(
        const CGvfReadRecord&,
        CVariation_ref&);

    bool xVariationMakeInsertions(
        const CGvfReadRecord&,
        CVariation_ref&);

    bool xVariationMakeDeletions(
        const CGvfReadRecord&,
        CVariation_ref&);

    bool xVariationMakeIndels(
        const CGvfReadRecord&,
        CVariation_ref&);

    bool xVariationMakeInversions(
        const CGvfReadRecord&,
        CVariation_ref&);

    bool xVariationMakeEversions(
        const CGvfReadRecord&,
        CVariation_ref&);

    bool xVariationMakeTranslocations(
        const CGvfReadRecord&,
        CVariation_ref&);

    bool xVariationMakeComplex(
        const CGvfReadRecord&,
        CVariation_ref&);

    bool xVariationMakeUnknown(
        const CGvfReadRecord&,
        CVariation_ref&);

    bool xVariationSetInsertions(
        const CGvfReadRecord&,
        CVariation_ref&);

    bool xVariationSetDeletions(
        const CGvfReadRecord&,
        CVariation_ref&);

    bool xVariationSetCommon(
        const CGvfReadRecord&,
        CVariation_ref&);

    virtual bool xVariationSetId(
        const CGvfReadRecord&,
        CVariation_ref&);

    virtual bool xVariationSetParent(
        const CGvfReadRecord&,
        CVariation_ref&);

    virtual bool xVariationSetName(
        const CGvfReadRecord&,
        CVariation_ref&);

    virtual bool xVariationSetSnvs(
        const CGvfReadRecord&,
        CVariation_ref&);

    virtual bool xVariationSetProperties(
        const CGvfReadRecord&,
        CVariation_ref&);

    CGff3ReadRecord* x_CreateRecord() override { return new CGvfReadRecord(m_uLineNumber); };

    bool xIsDbvarCall(
        const string& nameAttr) const;

    bool xGetNameAttribute(
        const CGvfReadRecord& record,
        string& name) const;

protected:
    CRef< CAnnotdesc > m_Pragmas;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // OBJTOOLS_READERS___GVF_READER__HPP
