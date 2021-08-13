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
 *   GFF3 file reader
 *
 */

#ifndef OBJTOOLS_READERS___GFF3_READER__HPP
#define OBJTOOLS_READERS___GFF3_READER__HPP

#include <corelib/ncbistd.hpp>
#include <objtools/readers/gff2_reader.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects) // namespace ncbi::objects::

class CGff3LocationMerger;

//  ============================================================================
class CGff3ReadRecord
//  ============================================================================
    : public CGff2Record
{
public:
    CGff3ReadRecord() {};
    ~CGff3ReadRecord() {};

    bool AssignFromGff(
        const string& ) override;

protected:
    string x_NormalizedAttributeKey(
        const string& );
};

//  ============================================================================
struct SAlignmentData
//  ============================================================================
{
    using MAP_ID_TO_ALIGN = map<string, list<CRef<CSeq_align>>>;
    MAP_ID_TO_ALIGN mAlignments;
    using LIST_IDS = list<string>;
    LIST_IDS mIds;

    void Reset() { mAlignments.clear(); mIds.clear(); };
    operator bool() const { return !mAlignments.empty(); };
};

//  ============================================================================
class NCBI_XOBJREAD_EXPORT CGff3Reader
//  ============================================================================
    : public CGff2Reader
{
    friend class CGff3ReadRecord;

public:
    //
    //  object management:
    //
public:
    enum {
        //range 12..23
        fGeneXrefs = (0x1 << 12),
    };
    typedef unsigned int TReaderFlags;

    CGff3Reader(
        unsigned int uFlags,
        const string& name = "",
        const string& title = "",
        SeqIdResolver resolver = CReadUtil::AsSeqId,
        CReaderListener* = nullptr);

    CGff3Reader(
        unsigned int uFlags,
        CReaderListener*);

    virtual ~CGff3Reader();

    CRef<CSeq_annot>
    ReadSeqAnnot(
        ILineReader& lr,
        ILineErrorListener* pErrors=nullptr) override;

    TSeqPos SequenceSize() const;

    TSeqPos GetSequenceSize(
        const string&) const;


protected:
    void xProcessData(
        const TReaderData&,
        CSeq_annot&) override;

    CGff3ReadRecord* x_CreateRecord() override { return new CGff3ReadRecord(); };

    virtual bool xInitializeFeature(
        const CGff2Record&,
        CRef<CSeq_feat> );

    bool xUpdateAnnotFeature(
        const CGff2Record&,
        CSeq_annot&,
        ILineErrorListener*) override;

    bool xAddFeatureToAnnot(
        CRef< CSeq_feat >,
        CSeq_annot& ) override;

    virtual bool xUpdateAnnotExon(
        const CGff2Record&,
        CRef<CSeq_feat>,
        CSeq_annot&,
        ILineErrorListener*);

    virtual bool xJoinLocationIntoRna(
        const CGff2Record&,
        ILineErrorListener*);

    virtual bool xUpdateAnnotCds(
        const CGff2Record&,
        CRef<CSeq_feat>,
        CSeq_annot&,
        ILineErrorListener*);

    virtual bool xUpdateAnnotGene(
        const CGff2Record&,
        CRef<CSeq_feat>,
        CSeq_annot&,
        ILineErrorListener*);

    virtual bool xUpdateAnnotRegion(
        const CGff2Record&,
        CRef<CSeq_feat>,
        CSeq_annot&,
        ILineErrorListener*);

    virtual bool xUpdateAnnotGeneric(
        const CGff2Record&,
        CRef<CSeq_feat>,
        CSeq_annot&,
        ILineErrorListener*);

    virtual bool xUpdateAnnotMrna(
        const CGff2Record&,
        CRef<CSeq_feat>,
        CSeq_annot&,
        ILineErrorListener*);

    virtual bool xFindFeatureUnderConstruction(
        const CGff2Record&,
        CRef<CSeq_feat>&);

    void xVerifyCdsParents(
        const CGff2Record&);

    virtual bool xFeatureSetXrefGrandParent(
        const string&,
        CRef<CSeq_feat>);

    virtual bool xFeatureSetXrefParent(
        const string&,
        CRef<CSeq_feat>);

    bool xReadInit() override;

    static string xNextGenericId();

    string xMakeRecordId(
        const CGff2Record& record);

    void xVerifyExonLocation(
        const string&,
        const CGff2Record&);

    bool xIsIgnoredFeatureType(
        const string&) override;

    virtual void xAddPendingExon(
        const string&,
        const CGff2Record&);

    virtual void xGetPendingExons(
        const string&,
        list<CGff2Record>&);

    void xPostProcessAnnot(
        CSeq_annot&) override;

    void xProcessAlignmentData(
        CSeq_annot& pAnnot);

    bool xParseFeature(
        const string&,
        CSeq_annot&,
        ILineErrorListener*) override;

    virtual bool xParseAlignment(
        const string& strLine);

    void xProcessSequenceRegionPragma(
        const string& pragma) override;

    // Data:
    map<string, string> mCdsParentMap;
    map<string, CRef<CSeq_interval> > mMrnaLocs;
    map<string, string> mIdToSeqIdMap;
    SAlignmentData mAlignmentData;

    using PENDING_EXONS = map<string, list<CGff2Record> >;
    PENDING_EXONS mPendingExons;

    unique_ptr<CGff3LocationMerger> mpLocations;
    static unsigned int msGenericIdCounter;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // OBJTOOLS_READERS___GFF3_READER__HPP
