/*
 * $Id$
 *
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
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistr.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objtools/readers/gff3_location_merger.hpp>
#include "reader_message_handler.hpp"
#include <objtools/readers/reader_listener.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);

//  ----------------------------------------------------------------------------
CGff3LocationRecord::CGff3LocationRecord(
    const CGff2Record& record,
    unsigned int flags,
    CGff3ReadRecord::SeqIdResolver seqIdResolve)
//  ----------------------------------------------------------------------------
{
    mpGffId = record.GetSeqId(flags, seqIdResolve);
    mStart = static_cast<TSeqPos>(record.SeqStart());
    mStop  = static_cast<TSeqPos>(record.SeqStop());
    mStrand = (record.IsSetStrand() ? record.Strand() : eNa_strand_plus);
    mType = record.NormalizedType();
    mPartNum = 0;
    string recordPart;
    if (record.GetAttribute("part", recordPart)) {
        try {
            mPartNum = NStr::StringToInt(recordPart);
        }
        catch (CStringException&) {
            //mPartNum = 0;
        }
    }
    mFrame = (mType == "cds" ? record.Phase() : CCdregion::eFrame_not_set);
    mSeqId = record.Id();
}


//  ============================================================================
bool
CGff3LocationRecord::ComparePositions(
    const CGff3LocationRecord& lhs,
    const CGff3LocationRecord& rhs)
//  ============================================================================
{
    if (lhs.mStrand == eNa_strand_minus) {
        return (lhs.mStart > rhs.mStart);
    }
    return (lhs.mStart < rhs.mStart);
}

CGffIdTracker::CGffIdTracker(CReaderListener* pListener)
    : m_pMessageListener(pListener)
{
}


//  ============================================================================
void CGffIdTracker::CheckAndIndexRecord(
    const CGff2Record& record)
//  ============================================================================
{
    string id;
    record.GetAttribute("ID", id);
    CheckAndIndexRecord(id, record);
}


//  ============================================================================
void CGffIdTracker::CheckAndIndexRecord(
    string id,
    const CGff2Record& record)
//  Add the given record to an index used for checking the validity og GFF3 IDs.
//  Will do ammediate chack against the records that are indexed already (in the
//  spirit of catching obvious problems early).
//  ============================================================================
{
    CReaderMessage errorDuplicateId(
        eDiag_Error,
        0,
        string("Bad data line: record ID \"") + id + "\" is used multiple times");

    CGffIdTrackRecord trackRecord(record);
    string parentId;
    record.GetAttribute("Parent", parentId);
    auto mapIt = mIds.find(id);
    if (mapIt == mIds.end()) {
        mapIt = mIds.emplace(id, list<CGffIdTrackRecord>()).first;
        mapIt->second.push_back(trackRecord);
        if (!parentId.empty()) {
            mParentIds.emplace(parentId);
        }
        return;
    }
    auto& recordList = mapIt->second;
    auto pendingType = record.NormalizedType();
    if (pendingType == "exon") {
        recordList.push_back(trackRecord);
        if (!parentId.empty()) {
            mParentIds.emplace(parentId);
        }
        return;
    } 

	if (!id.empty()) {
    	_ASSERT(!recordList.empty());
    	auto expectedType = recordList.front().mSeqType;
    	if (pendingType != expectedType) {
            if (m_pMessageListener) {
                m_pMessageListener->PutMessage(errorDuplicateId);
            } else {
        	    throw errorDuplicateId;
            }
    	}
    	auto pendingSeqId = record.Id();
    	auto expectedSeqId = recordList.front().mSeqId;
    	if (pendingSeqId != expectedSeqId) {
            if (m_pMessageListener) {
                m_pMessageListener->PutMessage(errorDuplicateId);
            } else {
        	    throw errorDuplicateId;
            }
    	}
	}
    if (!parentId.empty()) {
        mParentIds.emplace(parentId);
    }
    recordList.push_back(trackRecord);
}

//  ============================================================================
void CGffIdTracker::CheckIntegrity()
// Check validity of GFF3 feature and parent IDs based on the assumption that 
// all GFF3 records in the input have been seen and indexed.
// =============================================================================
{
    // make sure there is an ID for every ParentID:
    for (const auto& parentId: mParentIds) {
        if (mIds.find(parentId) == mIds.end()) {
            CReaderMessage errorBadParentId(
                eDiag_Error,
                0,
                string("Bad data line: Parent \"" + parentId + 
                    "\" does not refer to a GFF3 record ID"));
            if (m_pMessageListener) {
                m_pMessageListener->PutMessage(errorBadParentId);
            }
            else {
                throw errorBadParentId;
            }
        }
    }
}


//  ============================================================================
CGff3LocationMerger::CGff3LocationMerger(
    unsigned int flags,
    CGff3ReadRecord::SeqIdResolver idResolver,
    TSeqPos sequenceSize,
    CReaderListener* pListener):
//  ============================================================================
    mFlags(flags),
    mIdResolver(idResolver),
    mIdTracker(pListener),
    m_pMessageListener(pListener)
{
}

//  ============================================================================
void
CGff3LocationMerger::VerifyRecordLocation(
    const CGff2Record& record)
//  ============================================================================
{
    auto seqSizeIt = mSequenceSizes.find(record.Id());
    if (seqSizeIt == mSequenceSizes.end()) {
        return;
    }
    auto seqSize = seqSizeIt->second;
    if (seqSize == 0) {
        return; //pragma just gave ID, no size, hence useless here
    }

    // (1) in point better be less then seqSize:
    if (record.SeqStart() >= seqSize) {
        string message = "Bad data line: ";
        message += "feature in-point is outside the containing sequence.";
        CReaderMessage error(
            eDiag_Error,
            0,
            message);
        if (m_pMessageListener) {
            m_pMessageListener->PutMessage(error);    
        } else {
            throw error;
        }
    }
    // (2) no longer than sequence itself:
    if (record.SeqStop() - record.SeqStart() >= seqSize) {
        string message = "Bad data line: ";
        message += "feature is longer than the entire containing sequence.";
        CReaderMessage error(
            eDiag_Error,
            0,
            message);
        if (m_pMessageListener) {
            m_pMessageListener->PutMessage(error);
        } else {
            throw error;
        }
    }
}

//  ============================================================================
bool
CGff3LocationMerger::AddRecord(
    const CGff2Record& record)
//  ============================================================================
{
    mIdTracker.CheckAndIndexRecord(record);

    if (record.NormalizedType() == "cds") {
        VerifyRecordLocation(record);
        return true;
    }

    list<string> ids;
    if (!CGff3LocationMerger::xGetLocationIds(record, ids)) {
        return false;
    }

    for (const auto& id: ids) {
        AddRecordForId(id, record);
    }
    return true;
}

//  ============================================================================
void
CGff3LocationMerger::AddRecordForId(
    const string& id,
    const CGff2Record& record)
//  ============================================================================
{
    VerifyRecordLocation(record);

    auto existingEntry = mMapIdToLocations.find(id);
    if (existingEntry == mMapIdToLocations.end()) {
        existingEntry = mMapIdToLocations.emplace(id, LOCATIONS()).first;
    }
    LOCATIONS& locations = existingEntry->second;
    // special case: gene
    if (locations.size() == 1 && locations.front().mType == "gene") {
        return;
    }
    CGff3LocationRecord location(record, mFlags, mIdResolver);
    existingEntry->second.push_front(location);
}


//  ============================================================================
bool CGff3LocationMerger::xGetLocationIds(
    const CGff2Record& record,
    list<string>& ids)
//  ============================================================================
{
    string recordType = record.NormalizedType();

    if (NStr::EndsWith(recordType, "rna")) {
        return false;
    }
    if (NStr::EndsWith(recordType, "transcript")) {
        return false;
    }
    // prevent extra interval from getting into VDJC gene segment
    if (NStr::EndsWith(recordType, "_gene_segment")) {
        return false;
    }
    if (recordType == "exon" || 
        recordType == "five_prime_utr" || 
        recordType == "three_prime_utr") {
        return record.GetAttribute("Parent", ids);
    }
    if (record.GetAttribute("ID", ids)) {
        return true;
    }
    // create a temporary ID:
    if (!record.GetAttribute("Parent", ids)) {
        return false;
    }
    for (auto& id: ids) {
        id = record.Type() + ":" + id;
    }
    return true;
}

//  ============================================================================
void
CGff3LocationMerger::GetLocation(
    const string& id,
    CRef<CSeq_loc>& pSeqLoc,
    CCdregion::EFrame& frame)
//  ============================================================================
{
    auto it = mMapIdToLocations.find(id);
    if (it == mMapIdToLocations.end()) {
        pSeqLoc->Reset();
        return;
    }
    MergeLocation(pSeqLoc, frame, it->second);
}

//  ============================================================================
TSeqPos
CGff3LocationMerger::GetSequenceSize(
    const string& seqId) const
//  ============================================================================
{
    auto sizeIt = mSequenceSizes.find(seqId);
    if (sizeIt == mSequenceSizes.end()) {
        return 0;
    }
    return sizeIt->second;
}

//  ============================================================================
CRef<CSeq_loc>
CGff3LocationMerger::xGetRecordLocation(
    const CGff3LocationRecord& locRecord)
//  ============================================================================
{
    _ASSERT(locRecord.mpGffId);
    const auto& seqId = *(locRecord.mpGffId);

    CRef<CSeq_loc> pLocation(new CSeq_loc);
    TSeqPos sequenceSize = GetSequenceSize(locRecord.mSeqId);
    if (sequenceSize == 0) {
        CRef<CSeq_interval> pInterval(new CSeq_interval);
        pInterval->SetId().Assign(seqId);
        pInterval->SetFrom(locRecord.mStart);
        pInterval->SetTo(locRecord.mStop);
        pInterval->SetStrand(locRecord.mStrand);
        pLocation->SetInt(*pInterval);
        return pLocation;
    }

    if (locRecord.mStrand == eNa_strand_minus) {
        if (locRecord.mStart >= sequenceSize  || locRecord. mStop < sequenceSize) {
            CRef<CSeq_interval> pInterval(new CSeq_interval);
            pInterval->SetId().Assign(seqId);
            pInterval->SetFrom(locRecord.mStart % sequenceSize);
            pInterval->SetTo(locRecord.mStop % sequenceSize);
            pInterval->SetStrand(locRecord.mStrand);
            pLocation->SetInt(*pInterval);
        }
        else {
            CRef<CSeq_interval> pTop(new CSeq_interval);
            pTop->SetId().Assign(seqId);
            pTop->SetFrom(0);
            pTop->SetTo(locRecord.mStop % sequenceSize);
            pTop->SetStrand(locRecord.mStrand);
            pLocation->SetPacked_int().AddInterval(*pTop);
            CRef<CSeq_interval> pBottom(new CSeq_interval);
            pBottom->SetId().Assign(seqId);
            pBottom->SetFrom(locRecord.mStart % sequenceSize);
            pBottom->SetTo(sequenceSize - 1);
            pBottom->SetStrand(locRecord.mStrand);
            pLocation->SetPacked_int().AddInterval(*pBottom);
            pLocation->ChangeToMix();
        }
    }
    else {
        if (locRecord.mStart >= sequenceSize  ||  locRecord.mStop < sequenceSize) {
            CRef<CSeq_interval> pInterval(new CSeq_interval);
            pInterval->SetId().Assign(seqId);
            pInterval->SetFrom(locRecord.mStart % sequenceSize);
            pInterval->SetTo(locRecord.mStop % sequenceSize);
            pInterval->SetStrand(locRecord.mStrand);
            pLocation->SetInt(*pInterval);
        }
        else {
            CRef<CSeq_interval> pBottom(new CSeq_interval);
            pBottom->SetId().Assign(seqId);
            pBottom->SetFrom(locRecord.mStart % sequenceSize);
            pBottom->SetTo(sequenceSize - 1);
            pBottom->SetStrand(locRecord.mStrand);
            pLocation->SetPacked_int().AddInterval(*pBottom);
            CRef<CSeq_interval> pTop(new CSeq_interval);
            pTop->SetId().Assign(seqId);
            pTop->SetFrom(0);
            pTop->SetTo(locRecord.mStop % sequenceSize);
            pTop->SetStrand(locRecord.mStrand);
            pLocation->SetPacked_int().AddInterval(*pTop);
            pLocation->ChangeToMix();
        }
    }
    return pLocation;
}


//  ============================================================================
void
CGff3LocationMerger::MergeLocation(
    CRef<CSeq_loc>& pSeqLoc,
    CCdregion::EFrame& frame,
    LOCATIONS& locations)
//  ============================================================================
{
    if (locations.empty()) {
        pSeqLoc->SetNull();
        frame = CCdregion::eFrame_not_set;
        return;
    }
    if (locations.size() == 1) {
        auto& onlyOne = locations.front();
        pSeqLoc = xGetRecordLocation(onlyOne);
        frame = onlyOne.mFrame;
        return;
    }
    CGff3LocationMerger::xSortLocations(locations);
    auto& mix = pSeqLoc->SetMix();
    for (auto& location: locations) {
        mix.AddSeqLoc(*xGetRecordLocation(location));
    }
    pSeqLoc = pSeqLoc->Merge(CSeq_loc::fMerge_AbuttingOnly, nullptr);
    const auto& front = locations.front();
    frame = front.mFrame;
}


//  ============================================================================
void 
CGff3LocationMerger::Validate()
//  ============================================================================
{
    mIdTracker.CheckIntegrity();
}


//  =============================================================================
void
CGff3LocationMerger::xSortLocations(
    LOCATIONS& locations)
//  =============================================================================
{
    for (const auto& location: locations) {
        if (location.mPartNum == 0) {
            locations.sort(CGff3LocationRecord::ComparePositions);
            return;
        }
    }
    locations.sort(CGff3LocationRecord::ComparePartNumbers);
}


END_SCOPE(objects)
END_NCBI_SCOPE
