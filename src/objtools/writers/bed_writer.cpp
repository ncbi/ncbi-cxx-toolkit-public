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
 * Authors:  Frank Ludwig
 *
 * File Description:  Write bed file
 *
 */

#include <ncbi_pch.hpp>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/mapped_feat.hpp>

#include <objtools/writers/writer_exception.hpp>
#include <objtools/writers/write_util.hpp>
#include <objtools/writers/bed_track_record.hpp>
#include <objtools/writers/bed_feature_record.hpp>
#include <objtools/writers/bed_writer.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ----------------------------------------------------------------------------
class CThreeFeatRecord
//  ----------------------------------------------------------------------------
{
    friend class CThreeFeatManager;

public:
    CThreeFeatRecord() {};
    CThreeFeatRecord(
        const CSeq_feat&);
    ~CThreeFeatRecord() {};

    bool AddFeature(
        const CSeq_feat&);

    bool IsRecordComplete() const;

    bool
    GetBedFeature(
        CBedFeatureRecord&) const;
    
private:
    bool xAddFound(
        int);
    bool xAddAll(
        int);

    CRef<CSeq_feat> mpChrom;
    CRef<CSeq_feat> mpThick;
    CRef<CSeq_feat> mpBlocks;
    vector<int> mFeatsAll;
    vector<int> mFeatsFound;
};

//  ----------------------------------------------------------------------------
CThreeFeatRecord::CThreeFeatRecord(
    const CSeq_feat& feat)
//  ----------------------------------------------------------------------------
{
    AddFeature(feat);
}

//  ----------------------------------------------------------------------------
bool CThreeFeatRecord::AddFeature(
    const CSeq_feat& feature)
//  ----------------------------------------------------------------------------
{
    string threeFeatType;
    if (!feature.IsSetId()  ||  !feature.GetId().IsLocal()  
            ||  !feature.GetId().GetLocal().IsId()) {
        return false;
    }
    if (!CWriteUtil::GetThreeFeatType(feature, threeFeatType)) {
        return false;
    }
    bool assigned = false;
    if (threeFeatType == "chrom") {
        mpChrom.Reset(new CSeq_feat);
        mpChrom->Assign(feature);
        assigned = true;
    }
    if (threeFeatType == "thick") {
        mpThick.Reset(new CSeq_feat);
        mpThick->Assign(feature);
        assigned = true;
    }
    if (threeFeatType == "block") {
        mpBlocks.Reset(new CSeq_feat);
        mpBlocks->Assign(feature);
        assigned = true;
    }
    if (!assigned) {
        return false;
    }
    int featId = feature.GetId().GetLocal().GetId();
    xAddFound(featId);
    if (!feature.IsSetXref()) {
        return true;
    }
    for (CRef<CSeqFeatXref> pXref:  feature.GetXref()) {
        if (!pXref->IsSetId()  ||  !pXref->GetId().IsLocal()  || 
                !pXref->GetId().GetLocal().IsId()) {
            continue;
        }
        int featId = pXref->GetId().GetLocal().GetId();
        xAddAll(featId);
    }
    return true;
};

//  ----------------------------------------------------------------------------
bool CThreeFeatRecord::IsRecordComplete() const
//  ----------------------------------------------------------------------------
{
    return (mFeatsFound.size()  ==  mFeatsAll.size());
}

//  ----------------------------------------------------------------------------
bool
CThreeFeatRecord::GetBedFeature(
    CBedFeatureRecord& bedRecord) const
//  ----------------------------------------------------------------------------
{
    bedRecord = CBedFeatureRecord();
    if (!mpChrom) {
        return false;
    }
    if (!bedRecord.SetLocation(mpChrom->GetLocation())) {
        return false;
    }
    if (!bedRecord.SetName(mpChrom->GetData())) {
        return false;
    }
    int score;
    if (!CWriteUtil::GetThreeFeatScore(*mpChrom, score)) {
        score = 0;
    }
    if (!bedRecord.SetScore(score)) {
        return false;
    }
    if (mpThick) {
        if (!bedRecord.SetThick(mpThick->GetLocation())) {
            return false;
        }
    }
    else {
        if (!bedRecord.SetNoThick(mpChrom->GetLocation())) {
            return false;
        }
    }
    string color;
    if (CWriteUtil::GetThreeFeatRgb(*mpChrom, color)) {
        if (!bedRecord.SetRgb(color)) {
            return false;
        }
    }
    if (mpBlocks) {
        if (!bedRecord.SetBlocks(
                mpChrom->GetLocation(), mpBlocks->GetLocation())) {
            return false;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool
CThreeFeatRecord::xAddFound(
    int featId)
//
//  Expectation: featId is not listed as found already
//  ----------------------------------------------------------------------------
{
    vector<int>::iterator it = std::find(
        mFeatsFound.begin(), mFeatsFound.end(), featId);
    if (it != mFeatsFound.end()) {
        return false;
    }
    mFeatsFound.push_back(featId);
    return xAddAll(featId);
}

//  ----------------------------------------------------------------------------
bool
CThreeFeatRecord::xAddAll(
    int featId)
//  ----------------------------------------------------------------------------
{
    vector<int>::iterator it = std::find(
        mFeatsAll.begin(), mFeatsAll.end(), featId);
    if (it == mFeatsAll.end()) {
        mFeatsAll.push_back(featId);
    }
    return true;
}

//  ----------------------------------------------------------------------------
class CThreeFeatManager
//  ----------------------------------------------------------------------------
{
public:
    using RECORDS = vector<CThreeFeatRecord>;
    using RECORD_IT = RECORDS::iterator;

    CThreeFeatManager() {};
    ~CThreeFeatManager() {};

    bool
    AddFeature(
        const CSeq_feat&);

    bool
    IsRecordComplete(
        const CSeq_feat&);

    bool
    ProcessRecord(
        const CSeq_feat&,
        CBedFeatureRecord&);

    bool
    GetAnyRecord(
        CBedFeatureRecord&);

private:
    RECORD_IT
    xFindExistingRecord(
        const CSeq_feat&);

    RECORD_IT
    xAddRecord(
        const CSeq_feat&);

    RECORDS mRecords;
};

//  ----------------------------------------------------------------------------
bool CThreeFeatManager::AddFeature(
    const CSeq_feat& feature)
//  ----------------------------------------------------------------------------
{
    RECORD_IT it = xFindExistingRecord(feature);
    if (it == mRecords.end()) {
        RECORD_IT addIt = xAddRecord(feature);
        return (addIt != mRecords.end());
    }
    else {
        return it->AddFeature(feature);
    }
}

//  ----------------------------------------------------------------------------
bool CThreeFeatManager::IsRecordComplete(
    const CSeq_feat& feature)
//  ----------------------------------------------------------------------------
{
    RECORD_IT it = xFindExistingRecord(feature);
    if (it == mRecords.end()) {
        return false;
    }
    return (it->IsRecordComplete());
}

//  ----------------------------------------------------------------------------
bool
CThreeFeatManager::ProcessRecord(
    const CSeq_feat& feature,
    CBedFeatureRecord& bedRecord)
//  ----------------------------------------------------------------------------
{
    RECORD_IT it = xFindExistingRecord(feature);
    if (it == mRecords.end()) {
        return false;
    }
    if (!it->GetBedFeature(bedRecord)) {
        return false;
    }
    mRecords.erase(it);
    return true;
}

//  ----------------------------------------------------------------------------
bool
CThreeFeatManager::GetAnyRecord(
    CBedFeatureRecord& bedRecord)
//  ----------------------------------------------------------------------------
{
    if (mRecords.empty()) {
        return false;
    }
    RECORD_IT it = mRecords.end() - 1;
    if (!it->GetBedFeature(bedRecord)) {
        return false;
    }
    mRecords.erase(it);
    return true;
}
    
//  ----------------------------------------------------------------------------
CThreeFeatManager::RECORD_IT
CThreeFeatManager::xFindExistingRecord(
    const CSeq_feat& feature)
//  ----------------------------------------------------------------------------
{
    if (!feature.IsSetId()  ||  !feature.GetId().IsLocal()  
            ||  !feature.GetId().GetLocal().IsId()) {
        return mRecords.end();
    }
    int featId = feature.GetId().GetLocal().GetId();
    for (RECORD_IT it = mRecords.begin(); it != mRecords.end(); ++it) {
        vector<int>::iterator iit = std::find(
            it->mFeatsAll.begin(), it->mFeatsAll.end(), featId);
        if (iit != it->mFeatsAll.end()) {
            return it;
        }
    }
    return mRecords.end();
}

//  ----------------------------------------------------------------------------
CThreeFeatManager::RECORD_IT
CThreeFeatManager::xAddRecord(
    const CSeq_feat& feature)
//  ----------------------------------------------------------------------------
{
    CThreeFeatRecord threeFeatRecord;
    if (!threeFeatRecord.AddFeature(feature)) {
        return mRecords.end();
    }
    mRecords.push_back(threeFeatRecord);
    return (mRecords.end() - 1);
}

//  ----------------------------------------------------------------------------
CBedWriter::CBedWriter(
    CScope& scope,
    CNcbiOstream& ostr,
    unsigned int colCount,
    unsigned int uFlags ) :
//  ----------------------------------------------------------------------------
    CWriterBase(ostr, uFlags),
    m_Scope(scope),
    m_colCount(colCount)
{
    // the first three columns are mandatory
    if (m_colCount < 3) {
        m_colCount = 3;
    }
};

//  ----------------------------------------------------------------------------
CBedWriter::~CBedWriter()
//  ----------------------------------------------------------------------------
{
};

//  ----------------------------------------------------------------------------

//  ----------------------------------------------------------------------------
bool CBedWriter::WriteAnnot( 
    const CSeq_annot& annot,
    const string&,
    const string& )
//  ----------------------------------------------------------------------------
{
    if( annot.CanGetDesc() ) {
        ITERATE(CAnnot_descr::Tdata, DescIter, annot.GetDesc().Get()) {
            const CAnnotdesc& desc = **DescIter;
            if(desc.IsUser()) {
                if(desc.GetUser().HasField("NCBI_BED_COLUMN_COUNT")) {
                    CConstRef< CUser_field > field = desc.GetUser().GetFieldRef("NCBI_BED_COLUMN_COUNT");
                    if(field && field->CanGetData() && field->GetData().IsInt()) {
                        m_colCount = field->GetData().GetInt();
                    }
                }
            }
        }
    }
    
    CBedTrackRecord track;
    if ( ! track.Assign(annot) ) {
        return false;
    }
    track.Write(m_Os);

    if (CWriteUtil::IsThreeFeatFormat(annot)) {
        return xWriteAnnotThreeFeatData(track, annot);
    }
    else {
        return xWriteAnnotFeatureTable(track, annot);
    }
}


//  ----------------------------------------------------------------------------
bool CBedWriter::xWriteAnnotFeatureTable(
    const CBedTrackRecord& track,
    const CSeq_annot& annot)
//  ----------------------------------------------------------------------------
{
    SAnnotSelector sel = xGetAnnotSelector();
    CSeq_annot_Handle sah = m_Scope.AddSeq_annot(annot);
    for (CFeat_CI pMf(sah, sel); pMf; ++pMf ) {
        if (IsCanceled()) {
            NCBI_THROW(
                CObjWriterException,
                eInterrupted,
                "Processing terminated by user");
        }
        if (!xWriteFeature(track, *pMf)) {
            m_Scope.RemoveSeq_annot(sah);
            return false;
        }
    }
    m_Scope.RemoveSeq_annot(sah);
    return true;
}


//  ----------------------------------------------------------------------------
bool CBedWriter::xWriteAnnotThreeFeatData(
    const CBedTrackRecord& track,
    const CSeq_annot& annot)
//  ----------------------------------------------------------------------------
{
    CThreeFeatManager threeFeatManager;
    CBedFeatureRecord bedRecord;

    SAnnotSelector sel = xGetAnnotSelector();
    CSeq_annot_Handle sah = m_Scope.AddSeq_annot(annot);
    CFeat_CI pMf(sah, sel);
    for ( ; pMf; ++pMf ) {
        if (IsCanceled()) {
            NCBI_THROW(
                CObjWriterException,
                eInterrupted,
                "Processing terminated by user");
        }
        const CSeq_feat& feature = pMf->GetOriginalFeature();
        if (!threeFeatManager.AddFeature(feature)) {
            break;
        }
        if (!threeFeatManager.IsRecordComplete(feature)) {
            continue;
        }
        if (!threeFeatManager.ProcessRecord(feature, bedRecord)) {
            break;
        }
        if (!bedRecord.Write(m_Os, m_colCount)) {
            break;
        }
    }
    m_Scope.RemoveSeq_annot(sah);
    if (!pMf) {
        while (threeFeatManager.GetAnyRecord(bedRecord)) {
            continue;
        }
    }
    return (!pMf);
}



bool CBedWriter::xWriteFeature(
    const CMappedFeat& mapped_feat,
    CSeq_annot_Handle annot_handle,
    CBioseq_Handle dummy_arg)
{
    // Terribly inefficient. 
    // Store track and annot_handle, and only recreate track 
    // if the annot_handle has changed since the last call.
    CBedTrackRecord track;
    if ( ! track.Assign(*(annot_handle.GetCompleteSeq_annot())) ) {
        return false;
    }

    return xWriteFeature(track, mapped_feat);
}


//  ----------------------------------------------------------------------------
bool CBedWriter::xWriteFeature(
    const CBedTrackRecord& track,
    const CMappedFeat& mf)
//  ----------------------------------------------------------------------------
{
    CBedFeatureRecord record;
    if (!record.AssignDisplayData( mf, track.UseScore())) {
//          feature did not contain display data ---
//          Is there any alternative way to populate some of the bed columns?
//          For now, keep going, emit at least the locations ...
    }

    CRef<CSeq_loc> pPackedInt(new CSeq_loc(CSeq_loc::e_Mix));
    pPackedInt->Add(mf.GetLocation());
    CWriteUtil::ChangeToPackedInt(*pPackedInt);

    if (!pPackedInt->IsPacked_int() || !pPackedInt->GetPacked_int().CanGet()) {
        // nothing to do
        return true;
    }

    const list<CRef<CSeq_interval> >& sublocs = pPackedInt->GetPacked_int().Get();
    list<CRef<CSeq_interval> >::const_iterator it;
    for (it = sublocs.begin(); it != sublocs.end(); ++it ) {
        if (!record.AssignLocation(m_Scope, **it)  ||  !record.Write(m_Os, m_colCount)) {
            return false;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
SAnnotSelector CBedWriter::xGetAnnotSelector()
//  ----------------------------------------------------------------------------
{
    SAnnotSelector sel;
    sel.SetSortOrder(SAnnotSelector::eSortOrder_Normal);
    return sel;
}

END_NCBI_SCOPE
