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
 * Author:  Frank Ludwig
 *
 * File Description:
 *   GFF file reader
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <objtools/writers/gff_feature_record.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
CGffFeatureRecord::CGffFeatureRecord(
    const string& id):
//  ----------------------------------------------------------------------------
    CGffBaseRecord(id)
{};

//  ----------------------------------------------------------------------------
CGffFeatureRecord::CGffFeatureRecord(
    const CGffFeatureRecord& other):
//  ----------------------------------------------------------------------------
CGffBaseRecord(other){};

//  ----------------------------------------------------------------------------
CGffFeatureRecord::~CGffFeatureRecord()
//  ----------------------------------------------------------------------------
{};

//  ----------------------------------------------------------------------------
void CGffFeatureRecord::InitLocation(
    const CSeq_loc& loc)
//  ----------------------------------------------------------------------------
{
    m_pLoc.Reset(new CSeq_loc());
    m_pLoc->Assign(loc);
}

//  ----------------------------------------------------------------------------
void CGffFeatureRecord::SetEndpoints(
    unsigned int start,
    unsigned int stop,
    ENa_strand strand)
//  ----------------------------------------------------------------------------
{
    CGffBaseRecord::SetLocation(start, stop, strand);
}

//  ----------------------------------------------------------------------------
void CGffFeatureRecord::SetLocation(
    const CSeq_interval& interval,
    unsigned int wrapSize,
    unsigned int wrapPoint)
//  ----------------------------------------------------------------------------
{
    m_pLoc.Reset(new CSeq_loc());
    m_pLoc->SetInt().Assign(interval);
    if ( interval.CanGetFrom() ) {
        mSeqStart = interval.GetFrom();
    }
    if ( interval.CanGetTo() ) {
        mSeqStop = interval.GetTo();
    }
    if (wrapSize != 0 && mSeqStart < wrapPoint) {
        mSeqStart += wrapSize;
        mSeqStop += wrapSize;
    }
    unsigned int seqStart = Location().GetStart(eExtreme_Positional);
    unsigned int seqStop = Location().GetStop(eExtreme_Positional);
    string min = NStr::IntToString(seqStart + 1);
    string max = NStr::IntToString(seqStop + 1);
    if (Location().IsPartialStart(eExtreme_Biological)) {
        if (Location().GetStrand() == eNa_strand_minus) {
            SetAttribute("end_range", max + string(",."));
        }
        else {
            SetAttribute("start_range", string(".,") + min);
        }
    }
    if (Location().IsPartialStop(eExtreme_Biological)) {
        if (Location().GetStrand() == eNa_strand_minus) {
            SetAttribute("start_range", string(".,") + min);
        }
        else {
            SetAttribute("end_range", max + string(",."));
        }
    }

    if ( interval.IsSetStrand() ) {
        SetStrand(interval.GetStrand());
    }
}

//  ----------------------------------------------------------------------------
void CGffFeatureRecord::SetGbKeyFrom(
    CMappedFeat mf)
//  ----------------------------------------------------------------------------
{
    SetAttribute("gbkey", mf.GetData().GetKey());
}

//  ----------------------------------------------------------------------------
CGffAlignRecord::CGffAlignRecord(
    const string& id):
//  ----------------------------------------------------------------------------
    CGffBaseRecord(id),
    mRecordId(id),
    mGapIsTrivial(true),
    mAccumulatedMatches(0)
{
}

//  ----------------------------------------------------------------------------
string CGffAlignRecord::StrGap() const
//  ----------------------------------------------------------------------------
{
    return mAttrGap;
}

//  ----------------------------------------------------------------------------
void CGffAlignRecord::AddInsertion(
    unsigned int size)
//  ----------------------------------------------------------------------------
{
    FinalizeMatches();
    if (!mAttrGap.empty()) {
        mAttrGap += " ";
    }
    mAttrGap += "I";
    mAttrGap += NStr::IntToString(size);
    mGapIsTrivial = false;
}


//  ----------------------------------------------------------------------------
void CGffAlignRecord::AddForwardShift(
    unsigned int size)
//  ----------------------------------------------------------------------------
{
    FinalizeMatches();
    if (!mAttrGap.empty()) {
        mAttrGap += " ";
    }
    mAttrGap += "F";
    mAttrGap += NStr::IntToString(size);
    mGapIsTrivial = false;
}


//  ----------------------------------------------------------------------------
void CGffAlignRecord::AddReverseShift(
    unsigned int size)
//  ----------------------------------------------------------------------------
{
    FinalizeMatches();
    if (!mAttrGap.empty()) {
        mAttrGap += " ";
    }
    mAttrGap += "R";
    mAttrGap += NStr::IntToString(size);
    mGapIsTrivial = false;
}


//  ----------------------------------------------------------------------------
void CGffAlignRecord::AddDeletion(
    unsigned int size)
//  ----------------------------------------------------------------------------
{
    FinalizeMatches();
    if (!mAttrGap.empty()) {
        mAttrGap += " ";
    }
    mAttrGap += "D";
    mAttrGap += NStr::IntToString(size);
    mGapIsTrivial = false;
}


//  ----------------------------------------------------------------------------
void CGffAlignRecord::AddMatch(
    unsigned int size)
//  ----------------------------------------------------------------------------
{
    mAccumulatedMatches += size;
}

//  -----------------------------------------------------------------------------
void CGffAlignRecord::FinalizeMatches()
//  -----------------------------------------------------------------------------
{
    if (mAccumulatedMatches != 0) {
        if (!mAttrGap.empty()) {
            mAttrGap += " ";
        }
        mAttrGap += "M";
        mAttrGap += NStr::IntToString(mAccumulatedMatches);
        mAccumulatedMatches = 0;
    }
    if (!mGapIsTrivial) {
        SetAttribute("Gap", mAttrGap);
    }
}

//  -----------------------------------------------------------------------------
string CGffAlignRecord::StrAttributes() const
//  -----------------------------------------------------------------------------
{
    string attributes;
    attributes.reserve(256);

    if (!mRecordId.empty()) {
        attributes += "ID=";
        attributes += mRecordId;
    }
    auto baseAttributes = CGffBaseRecord::StrAttributes();
    if (!baseAttributes.empty()) {
        attributes +=  ATTR_SEPARATOR;
        attributes += baseAttributes;
    }
    return attributes;
}

END_objects_SCOPE
END_NCBI_SCOPE
