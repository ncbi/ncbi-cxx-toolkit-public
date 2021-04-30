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
    mType("."),
    mMethod("."),
    mSeqStart(0),
    mSeqStop(0),
    mScore("."),
    mStrand("."),
    mPhase(".")
//  ----------------------------------------------------------------------------
{
}

//  ----------------------------------------------------------------------------
CGffBaseRecord::CGffBaseRecord(
    const CGffBaseRecord& other):
//  ----------------------------------------------------------------------------
    mSeqId(other.mSeqId),
    mType(other.mType),
    mMethod(other.mMethod),
    mSeqStart(other.mSeqStart),
    mSeqStop(other.mSeqStop),
    mScore(other.mScore),
    mStrand(other.mStrand),
    mPhase(other.mPhase)
{
    m_pLoc = other.m_pLoc;
    mAttributes.insert( 
        other.mAttributes.begin(), other.mAttributes.end());
}

//  ----------------------------------------------------------------------------
CGffBaseRecord::~CGffBaseRecord()
//  ----------------------------------------------------------------------------
{
}

//  ----------------------------------------------------------------------------
bool CGffBaseRecord::AddAttribute(
    const string& key,
    const string& value )
//  ----------------------------------------------------------------------------
{
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
    const string& attr)
//  ----------------------------------------------------------------------------
{
    TAttrIt it = mAttributes.find(attr);
    if (it == mAttributes.end()) {
        return false;
    }
    mAttributes.erase(it);
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
string CGffBaseRecord::StrSeqId() const
//  ----------------------------------------------------------------------------
{
    return xEscapedString(mSeqId);
}

//  ----------------------------------------------------------------------------
string CGffBaseRecord::StrMethod() const
//  ----------------------------------------------------------------------------
{
    return xEscapedString(mMethod);
}

//  ----------------------------------------------------------------------------
string CGffBaseRecord::StrType() const
//  ----------------------------------------------------------------------------
{
    return xEscapedString(mType);
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
    case objects::eNa_strand_minus:
        mStrand = "-";
        break;
    case objects::eNa_strand_other:
        mStrand = "?";
        break;
    case objects::eNa_strand_both:
    case objects::eNa_strand_both_rev:
        mStrand = ".";
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
        return;
    }

    //mss-263: if a record has multiple scores with the same key, turn them into
    // a comma separated multi-value
    CGffBaseRecord::TScoreIt it = mExtraScores.find(key);
    if (it != mExtraScores.end()) {
        value = it->second + "," + value;
    }
    mExtraScores[key] = value;
}

//  ----------------------------------------------------------------------------
void CGffBaseRecord::SetScore(
    const string& score)
//  ----------------------------------------------------------------------------
{
    mScore = score;
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
bool lessAttrCit(
    const CGffBaseRecord::TAttrCit& lhs, const CGffBaseRecord::TAttrCit& rhs)
//  ----------------------------------------------------------------------------
{
    return (lhs->first < rhs->first);
}

//  ----------------------------------------------------------------------------
bool lessScoreCit(
    const CGffBaseRecord::TScoreCit& lhs, const CGffBaseRecord::TScoreCit& rhs)
//  ----------------------------------------------------------------------------
{
    return (lhs->first < rhs->first);
}

//  ----------------------------------------------------------------------------
string CGffBaseRecord::StrAttributes() const
//  ----------------------------------------------------------------------------
{
    //mss-264: Order of attributes:
    //  ID always comes first
    //  Parent is next if present
    //  Then come all regular attributes EXCEPT FOR Gap, in alphabetical order
    //  Then come any extra scores, in alphabetical order
    //  Finally and always last is Gap if it is present at all
    //

    string attributes;
	attributes.reserve(256);

    typedef vector<TAttrCit> SORTATTRS;
    TAttrCit gapAttr = mAttributes.end();
    SORTATTRS sortedAttrs;
    sortedAttrs.reserve(mAttributes.size());
    for (TAttrCit ait = mAttributes.begin(); ait != mAttributes.end(); ++ait) {
        if (ait->first != "Gap") {
            sortedAttrs.push_back(ait);
        }
        else {
            gapAttr = ait;
        }
    }
    sort(sortedAttrs.begin(), sortedAttrs.end(), lessAttrCit);
    for (SORTATTRS::const_iterator cit = sortedAttrs.begin(); 
            cit != sortedAttrs.end(); ++cit) { 
        const string& key = (*cit)->first;

        if (!attributes.empty()) {
            attributes += ATTR_SEPARATOR;
        }
        attributes += xEscapedString(key);
        attributes += "=";
        bool got_values = false;
        for (auto vit : (*cit)->second) 
        {
            auto singleValue = xEscapedValue(key, vit);
            if (!singleValue.empty()) {
                if (got_values)
                    attributes += ",";
                attributes += singleValue;
                got_values = true;
            }
        }
    }

    typedef vector<TScoreCit> SORTSCORES;
    SORTSCORES sortedScores;
    sortedScores.reserve(mExtraScores.size());
    for (TScoreCit ait = mExtraScores.begin(); ait != mExtraScores.end(); ++ait) {
        sortedScores.push_back(ait);
    }
   
    sort(sortedScores.begin(), sortedScores.end(), lessScoreCit);
    for (auto cit : sortedScores) 
    { 
        const string& key = cit->first;

        if (!attributes.empty()) {
            attributes += ATTR_SEPARATOR;
        }
        attributes += xEscapedString(key);
        attributes += "=";	
		attributes += xEscapedValue(key, cit->second);
    }
    if (gapAttr != mAttributes.end()) {
        const string& key = gapAttr->first;

        if (!attributes.empty()) {
            attributes += ATTR_SEPARATOR;
        }
        attributes += xEscapedString(key);
        attributes += "=";
		attributes += xEscapedValue(key, gapAttr->second[0]);
    }
    if ( attributes.empty() ) {
        attributes = ".";
    }
    return attributes;
}

//  ----------------------------------------------------------------------------
const string& CGffBaseRecord::xEscapedValue(
    const string& key,
    const string& value) const
//  ----------------------------------------------------------------------------
{
    // unfortunately, const_bitset has no better method to set the values yet, maybe improve it later
    using charset = ct::const_bitset<256, char>;
    static constexpr auto escaped_no_range    = charset{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
        '%', ';', '=', '%', '=', '&', 0x7F, ',' };
    static constexpr auto escaped_range_value = charset{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
        '%', ';', '=', '%', '=', '&', 0x7F };

    const auto& escaped = (key == "start_range" || key == "end_range") ? escaped_range_value : escaped_no_range;

    // we need it really fast
    static constexpr char hexa_numbers[] = "0123456789ABCDEF";

    char replacement[4];
    replacement[0] = '%';
    replacement[3] = 0;

    // this is the local storage for escaped string
    static thread_local string result;
    result.clear();

    // last_pos show how many of the original input has been added to the result
    size_t last_pos = 0;
    for (size_t i = 0; i < value.size(); ++i)
    {
        char c = value[i];
        if (escaped.test(c))
        {
            replacement[1] = hexa_numbers[c / 16];
            replacement[2] = hexa_numbers[c % 16];
            result.append(value.data() + last_pos, i - last_pos);
            result.append(replacement, 3);
            last_pos = i+1;
        }
    }
    if (last_pos == 0) {
        return value;
    } else {
        result.append(value.data() + last_pos, value.size() - last_pos);
        return result;
    }
}

//  ----------------------------------------------------------------------------
ENa_strand CGffBaseRecord::SeqStrand() const
//  ----------------------------------------------------------------------------
{
    switch (this->mStrand[0]) {
        default:
            return eNa_strand_plus;
        case '-':
            return eNa_strand_minus;
        case '?':
            return eNa_strand_other;
    }
}
END_objects_SCOPE
END_NCBI_SCOPE
