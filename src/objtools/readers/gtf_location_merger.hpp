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
 * Authors: Frank Ludwig
 *
 */

#ifndef _GTF_LOCATION_MERGER_HPP_
#define _GTF_LOCATION_MERGER_HPP_

#include <corelib/ncbistd.hpp>
#include <objtools/readers/gtf_reader.hpp>
#include <objtools/readers/gff3_reader.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);

//class CGff2Record;

//  ============================================================================
class CGtfLocationRecord
//  ============================================================================
{
public:
    CGtfLocationRecord(
        const CGtfReadRecord&,
        unsigned int,
        CGff3ReadRecord::SeqIdResolver);

    CGtfLocationRecord(
        const CGtfLocationRecord&);

    CGtfLocationRecord&
    operator=(
        const CGtfLocationRecord&);

    CRef<CSeq_loc> GetLocation(
        TSeqPos sequenceSize);

    static bool ComparePartNumbers(
        const CGtfLocationRecord& lhs,
        const CGtfLocationRecord& rhs) { return lhs.mPartNum < rhs.mPartNum; };

    bool
    Contains(
        const CGtfLocationRecord&) const;
    bool
    IsContainedBy(
        const CGtfLocationRecord&) const;

    CSeq_id mId;
    TSeqPos mStart;
    TSeqPos mStop;
    ENa_strand mStrand;
    string mType;
    size_t mPartNum; 

};

//  ============================================================================
class CGtfLocationMerger
//  ============================================================================
{
    using LOCATIONS = list<CGtfLocationRecord>;
    using LOCATION_MAP = map<string, LOCATIONS>;

public:
    CGtfLocationMerger(
        unsigned int flags =0,
        CGff3ReadRecord::SeqIdResolver =nullptr,
        TSeqPos sequenceSize =0);

    void SetSequenceSize(
        TSeqPos sequenceSize) { mSequenceSize = sequenceSize; }

    bool AddRecord(
        const CGtfReadRecord&);

    void AddRecordForId(
        const string&,
        const CGtfReadRecord&);

    void AddStubForId(
        const string&);

    LOCATION_MAP& LocationMap() { return mMapIdToLocations; }

    CRef<CSeq_loc> MergeLocation(
        CSeqFeatData::ESubtype,
        LOCATIONS&);

    CRef<CSeq_loc> MergeLocationDefault(
        LOCATIONS&);
    CRef<CSeq_loc> MergeLocationForCds(
        LOCATIONS&);
    CRef<CSeq_loc> MergeLocationForTranscript(
        LOCATIONS&);
    CRef<CSeq_loc> MergeLocationForGene(
        LOCATIONS&);

    string GetFeatureIdFor(
        const CGtfReadRecord&,
        const string& =""); //prefix override
private:
    static bool xGetLocationIds(
        const CGtfReadRecord&,
        list<string>&);

    TSeqPos mSequenceSize;
    unsigned int mFlags;
    CGff3ReadRecord::SeqIdResolver mIdResolver;

    LOCATION_MAP mMapIdToLocations;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // _GTF_LOCATION_MERGER_HPP_
