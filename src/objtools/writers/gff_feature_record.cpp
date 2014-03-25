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

#include <objtools/writers/write_util.hpp>
#include <objtools/writers/gff_feature_record.hpp>
#include <objmgr/mapped_feat.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>

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
void CGffFeatureRecord::SetLocation(
    const CSeq_interval& interval) 
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
CGffAlignRecord::CGffAlignRecord(
    const string& id):
//  ----------------------------------------------------------------------------
    CGffBaseRecord(id),
    mGapIsTrivial(true),
    mAccumulatedMatches(0)
{
}

//  ----------------------------------------------------------------------------
void CGffAlignRecord::SetTarget(
    const string& target)
//  ----------------------------------------------------------------------------
{
    mAttrTarget = target;
}

//  ----------------------------------------------------------------------------
string CGffAlignRecord::StrTarget() const
//  ----------------------------------------------------------------------------
{
    return mAttrTarget;
}

//  ----------------------------------------------------------------------------
string CGffAlignRecord::StrGap() const
//  ----------------------------------------------------------------------------
{
    return mAttrGap;
}

//  ----------------------------------------------------------------------------
string CGffAlignRecord::StrAttributes() const
//  ----------------------------------------------------------------------------
{
    string attributes = CGffBaseRecord::StrAttributes();
    attributes += CGffBaseRecord::ATTR_SEPARATOR;
    attributes += "Target=";
    attributes += StrTarget();
    if (!mGapIsTrivial) {
        attributes += CGffBaseRecord::ATTR_SEPARATOR;
        attributes += "Gap=";
        attributes += StrGap();
    }
    return attributes;
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
    if (mAccumulatedMatches == 0) {
        return;
    }
    if (!mAttrGap.empty()) {
        mAttrGap += " ";
    }
    mAttrGap += "M";
    mAttrGap += NStr::IntToString(mAccumulatedMatches);
    mAccumulatedMatches = 0;
}

END_objects_SCOPE
END_NCBI_SCOPE
