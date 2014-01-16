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
#include <objtools/writers/gff_base_record.hpp>

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
const char* CGffBaseRecord::ATTR_SEPARATOR
//  ----------------------------------------------------------------------------
    = ";";

//  ----------------------------------------------------------------------------
CGffBaseRecord::CGffBaseRecord(
    const string& id):
//  ----------------------------------------------------------------------------
    mSeqId(""),
    mSeqStart(0),
    mSeqStop(0),
    mMethod("."),
    mType("."),
    mScore("."),
    mStrand("."),
    mPhase(".")
//  ----------------------------------------------------------------------------
{
    if (!id.empty()) {
        SetRecordId(id);
    }
};

//  ----------------------------------------------------------------------------
CGffBaseRecord::CGffBaseRecord(
    const CGffBaseRecord& other):
//  ----------------------------------------------------------------------------
    mSeqId(other.mSeqId),
    mSeqStart(other.mSeqStart),
    mSeqStop(other.mSeqStop),
    mMethod(other.mMethod),
    mType(other.mType),
    mScore(other.mScore),
    mStrand(other.mStrand),
    mPhase(other.mPhase)
{
    m_pLoc = other.m_pLoc;
    mAttributes.insert( 
        other.mAttributes.begin(), other.mAttributes.end());
};

//  ----------------------------------------------------------------------------
CGffBaseRecord::~CGffBaseRecord()
//  ----------------------------------------------------------------------------
{
};

//  ----------------------------------------------------------------------------
bool CGffBaseRecord::AddAttribute(
    const string& key,
    const string& value )
//  ----------------------------------------------------------------------------
{
    if (value.empty()) {
        return false; //don't accept blank values 
    }
    TAttrIt it = mAttributes.find(key);
    if (it == mAttributes.end()) {
        mAttributes[key] = vector<string>();
    }
    if (std::find(mAttributes[key].begin(), mAttributes[key].end(), value) == 
            mAttributes[key].end()) {
        mAttributes[key].push_back(value);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffBaseRecord::SetAttribute(
    const string& key,
    const string& value )
//  ----------------------------------------------------------------------------
{
    DropAttributes(key);
    return AddAttribute(key, value);
}

//  ----------------------------------------------------------------------------
bool CGffBaseRecord::GetAttributes(
    const string& key,
    vector<string>& value ) const
//  ----------------------------------------------------------------------------
{
    TAttrCit it = mAttributes.find(key);
    if (it == mAttributes.end()  ||  it->second.empty()) {
        return false;
    }
    value = it->second;
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffBaseRecord::AddAttributes(
    const string& key,
    const vector<string>& values)
//  ----------------------------------------------------------------------------
{
    if (values.empty()) {
        return true; //nothing to do 
    }
    TAttrIt it = mAttributes.find(key);
    if (it == mAttributes.end()) {
        mAttributes[key] = vector<string>(values.begin(), values.end());
        return true;
    }
    for (vector<string>::const_iterator cit = values.begin(); 
            cit != values.end(); ++cit) {
        string current = *cit;
        vector<string>::iterator iit = std::find(
            mAttributes[key].begin(), mAttributes[key].end(), current);
        if (iit == mAttributes[key].end()) {
            mAttributes[key].push_back(current);
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffBaseRecord::DropAttributes(
    const string& strAttr )
//  ----------------------------------------------------------------------------
{
    TAttrIt it = mAttributes.find( strAttr );
    if ( it == mAttributes.end() ) {
        return false;
    }
    mAttributes.erase( it );
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffBaseRecord::SetAttributes(
    const string& key,
    const vector<string>& values)
//  ----------------------------------------------------------------------------
{
    mAttributes[key] = vector<string>(values.begin(), values.end());
    return true;
}

//  ----------------------------------------------------------------------------
void CGffBaseRecord::SetRecordId(
    const string& recordId)
//  ----------------------------------------------------------------------------
{
    SetAttribute("ID", recordId);
}

//  ----------------------------------------------------------------------------
string CGffBaseRecord::StrSeqId() const
//  ----------------------------------------------------------------------------
{
    return mSeqId;
}

//  ----------------------------------------------------------------------------
string CGffBaseRecord::StrMethod() const
//  ----------------------------------------------------------------------------
{
    return mMethod;
}

//  ----------------------------------------------------------------------------
string CGffBaseRecord::StrType() const
//  ----------------------------------------------------------------------------
{
    return mType;
}

//  ----------------------------------------------------------------------------
void CGffBaseRecord::SetSeqId(
    const string& seqId) 
//  ----------------------------------------------------------------------------
{
    mSeqId = seqId;
}

//  ----------------------------------------------------------------------------
void CGffBaseRecord::SetMethod(
    const string& method)
//  ----------------------------------------------------------------------------
{
    mMethod = method;
}

//  ----------------------------------------------------------------------------
void CGffBaseRecord::SetType(
    const string& type)
//  ----------------------------------------------------------------------------
{
    mType = type;
}

//  ----------------------------------------------------------------------------
void CGffBaseRecord::SetLocation(
    unsigned int seqStart,
    unsigned int seqStop,
    ENa_strand seqStrand)
//  ----------------------------------------------------------------------------
{
    mSeqStart = seqStart;
    mSeqStop = seqStop;
    SetStrand(seqStrand);
}

//  ----------------------------------------------------------------------------
void CGffBaseRecord::SetStrand(
    ENa_strand seqStrand)
//  ----------------------------------------------------------------------------
{
    switch(seqStrand) {
    default:
        mStrand = "+";
        break;
    case objects::eNa_strand_unknown:
        mStrand = ".";
        break;
    case objects::eNa_strand_minus:
        mStrand = "-";
        break;
    }
}

//  ----------------------------------------------------------------------------
void CGffBaseRecord::SetScore(
    const CScore& score)
//  ----------------------------------------------------------------------------
{
    if (!score.IsSetId()  ||  !score.GetId().IsStr()) {
        return;
    }
    if (!score.IsSetValue()) {
        return;
    }
    string key = score.GetId().GetStr();
    string value;
    if (score.GetValue().IsInt()) {
        value = NStr::IntToString(score.GetValue().GetInt());
    }
    else if (score.GetValue().IsReal()) {
        value = NStr::DoubleToString(score.GetValue().GetReal());
    }
    else {
        return;
    }
    if (key == "score") {
        mScore = value;
    }
    else {
        mExtraScores[key] = value;
    }
}

//  ----------------------------------------------------------------------------
void CGffBaseRecord::SetPhase(
    unsigned int phase )
//  ----------------------------------------------------------------------------
{
    mPhase = NStr::IntToString((3+phase)%3);
}

//  ----------------------------------------------------------------------------
string CGffBaseRecord::StrScore() const
//  ----------------------------------------------------------------------------
{
    return mScore;
}

//  ----------------------------------------------------------------------------
string CGffBaseRecord::StrSeqStart() const
//  ----------------------------------------------------------------------------
{
    return NStr::UIntToString(mSeqStart + 1);;
}

//  ----------------------------------------------------------------------------
string CGffBaseRecord::StrSeqStop() const
//  ----------------------------------------------------------------------------
{
    return NStr::UIntToString(mSeqStop + 1);
}

//  ----------------------------------------------------------------------------
string CGffBaseRecord::StrStrand() const
//  ----------------------------------------------------------------------------
{
    return mStrand;
}

//  ----------------------------------------------------------------------------
string CGffBaseRecord::StrPhase() const
//  ----------------------------------------------------------------------------
{
    return mPhase;
}

//  ----------------------------------------------------------------------------
string CGffBaseRecord::StrAttributes() const
//  ----------------------------------------------------------------------------
{
    string attributes;
	attributes.reserve(256);

    for (TAttrCit it = mAttributes.begin(); it != mAttributes.end(); ++it) {
        string key = it->first;

        if (!attributes.empty()) {
            attributes += ATTR_SEPARATOR;
        }
        attributes += key;
        attributes += "=";
		
        vector<string> escapedValues;
        for (vector<string>::const_iterator cit = it->second.begin();
                cit != it->second.end(); ++cit) {
            escapedValues.push_back(xEscapedValue(key, *cit));
        }
        string value = NStr::Join(escapedValues, ",");
		attributes += value;
    }

    for (TScoreCit it = mExtraScores.begin(); it != mExtraScores.end(); ++it) {
        string key = it->first;

        if (!attributes.empty()) {
            attributes += ATTR_SEPARATOR;
        }
        attributes += key;
        attributes += "=";
		
        string value = it->second;
		attributes += xEscapedValue(key, value);
    }
    if ( attributes.empty() ) {
        attributes = ".";
    }
    return attributes;
}

//  ----------------------------------------------------------------------------
string CGffBaseRecord::xEscapedValue(
    const string& key,
    const string& value) const
//  ----------------------------------------------------------------------------
{
    string escapedValue(value);
    return value;
}

END_objects_SCOPE
END_NCBI_SCOPE
