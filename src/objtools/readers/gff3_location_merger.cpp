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
#include "gff3_location_merger.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);

//  ----------------------------------------------------------------------------
CGff3LocationRecord::CGff3LocationRecord(
    const CGff2Record& record,
    unsigned int flags,
    CGff3ReadRecord::SeqIdResolver seqIdResolve)
//  ----------------------------------------------------------------------------
{
    mId.Assign(*record.GetSeqId(flags, seqIdResolve));
    mStart = static_cast<TSeqPos>(record.SeqStart());
    mStop  = static_cast<TSeqPos>(record.SeqStop());
    mStrand = (record.IsSetStrand() ? record.Strand() : eNa_strand_plus);
    mType = record.Type();
    NStr::ToLower(mType);
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
}

//  ----------------------------------------------------------------------------
CGff3LocationRecord::CGff3LocationRecord(
    const CGff3LocationRecord& other)
//  ----------------------------------------------------------------------------
{
    mId.Assign(other.mId);
    mStart = other.mStart;
    mStop = other.mStop;
    mStrand = other.mStrand;
    mType = other.mType;
    mPartNum = other.mPartNum;
}

//  ----------------------------------------------------------------------------
CRef<CSeq_loc> 
CGff3LocationRecord::GetLocation(
    TSeqPos sequenceSize)
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_loc> pLocation(new CSeq_loc);

    if (sequenceSize == 0) {
        CRef<CSeq_interval> pInterval(new CSeq_interval);
        pInterval->SetId().Assign(mId);
        pInterval->SetFrom(mStart);
        pInterval->SetTo(mStop);
        pInterval->SetStrand(mStrand);
        pLocation->SetInt(*pInterval);
        return pLocation;
    }

    if (mStrand == eNa_strand_minus) {
        if (mStart >= sequenceSize  ||  mStop < sequenceSize) {
            CRef<CSeq_interval> pInterval(new CSeq_interval);
            pInterval->SetId().Assign(mId);
            pInterval->SetFrom(mStart % sequenceSize);
            pInterval->SetTo(mStop % sequenceSize);
            pInterval->SetStrand(mStrand);
            pLocation->SetInt(*pInterval);
        }
        else {
            CRef<CSeq_interval> pTop(new CSeq_interval);
            pTop->SetId().Assign(mId);
            pTop->SetFrom(0);
            pTop->SetTo(mStop % sequenceSize);
            pTop->SetStrand(mStrand);
            pLocation->SetPacked_int().AddInterval(*pTop);
            CRef<CSeq_interval> pBottom(new CSeq_interval);
            pBottom->SetId().Assign(mId);
            pBottom->SetFrom(mStart % sequenceSize);
            pBottom->SetTo(sequenceSize - 1);
            pBottom->SetStrand(mStrand);
            pLocation->SetPacked_int().AddInterval(*pBottom);
            pLocation->ChangeToMix();
        }
    }
    else {
        if (mStart >= sequenceSize  ||  mStop < sequenceSize) {
            CRef<CSeq_interval> pInterval(new CSeq_interval);
            pInterval->SetId().Assign(mId);
            pInterval->SetFrom(mStart % sequenceSize);
            pInterval->SetTo(mStop % sequenceSize);
            pInterval->SetStrand(mStrand);
            pLocation->SetInt(*pInterval);
        }
        else {
            CRef<CSeq_interval> pBottom(new CSeq_interval);
            pBottom->SetId().Assign(mId);
            pBottom->SetFrom(mStart % sequenceSize);
            pBottom->SetTo(sequenceSize - 1);
            pBottom->SetStrand(mStrand);
            pLocation->SetPacked_int().AddInterval(*pBottom);
            CRef<CSeq_interval> pTop(new CSeq_interval);
            pTop->SetId().Assign(mId);
            pTop->SetFrom(0);
            pTop->SetTo(mStop % sequenceSize);
            pTop->SetStrand(mStrand);
            pLocation->SetPacked_int().AddInterval(*pTop);
            pLocation->ChangeToMix();
        }
    }
    return pLocation;
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


//  ============================================================================
CGff3LocationMerger::CGff3LocationMerger(
    unsigned int flags,
    CGff3ReadRecord::SeqIdResolver idResolver,
    TSeqPos sequenceSize):
//  ============================================================================
    mFlags(flags),
    mSequenceSize(sequenceSize),
    mIdResolver(idResolver)
{
}

//  ============================================================================
bool
CGff3LocationMerger::AddRecord(
    const CGff2Record& record)
//  ============================================================================
{
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
    auto existingEntry = mMapIdToLocations.find(id);
    if (existingEntry == mMapIdToLocations.end()) {
        existingEntry = mMapIdToLocations.emplace(id, LOCATIONS()).first;
    }
    LOCATIONS& locations = existingEntry->second;
    // special case: gene
    if (locations.size() == 1  &&  locations.front().mType == "gene") {
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
    string recordType = record.Type();
    NStr::ToLower(recordType);

    if (NStr::EndsWith(recordType, "rna")) {
        return false;
    }
    if (NStr::EndsWith(recordType, "transcript")) {
        return false;
    }
    if (recordType == "exon") {
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
CRef<CSeq_loc>
CGff3LocationMerger::GetLocation(
    const string& id)
//  ============================================================================
{
    auto it = mMapIdToLocations.find(id);
    if (it == mMapIdToLocations.end()) { 
        return CRef<CSeq_loc>();
    }
    return MergeLocation(it->second);
}

//  ============================================================================
CRef<CSeq_loc> 
CGff3LocationMerger::MergeLocation(
    LOCATIONS& locations)
//  ============================================================================
{
    CRef<CSeq_loc> pSeqloc(new CSeq_loc);
    if (locations.empty()) {
        pSeqloc->SetNull();
        return pSeqloc;
    }
    if (locations.size() == 1) {
        auto& onlyOne = locations.front();
        pSeqloc = onlyOne.GetLocation(mSequenceSize); 
        return pSeqloc;
    }
    CGff3LocationMerger::xSortLocations(locations);
    auto& mix = pSeqloc->SetMix();
    for (auto& location: locations) {
        mix.AddSeqLoc(*location.GetLocation(mSequenceSize));
    }
    return pSeqloc;
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
