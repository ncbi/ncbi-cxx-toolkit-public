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
const char* CGffFeatureRecord::ATTR_SEPARATOR
//  ----------------------------------------------------------------------------
    = "; ";

//  ----------------------------------------------------------------------------
string CGffFeatureRecord::StrSeqId() const
//  ----------------------------------------------------------------------------
{
    return m_strSeqId;
}

//  ----------------------------------------------------------------------------
string CGffFeatureRecord::StrSource() const
//  ----------------------------------------------------------------------------
{
    return m_strSource;
}

//  ----------------------------------------------------------------------------
CGffFeatureRecord::CGffFeatureRecord(
    const string& id):
//  ----------------------------------------------------------------------------
    m_strSeqId(""),
    m_uSeqStart(0),
    m_uSeqStop(0),
    m_strSource("."),
    m_strType("."),
    m_pScore(0),
    m_peStrand(0),
    m_puPhase(0)
//  ----------------------------------------------------------------------------
{
    if (!id.empty()) {
        SetRecordId(id);
    }
};

//  ----------------------------------------------------------------------------
CGffFeatureRecord::CGffFeatureRecord(
    const CGffFeatureRecord& other):
//  ----------------------------------------------------------------------------
    m_strSeqId(other.m_strSeqId),
    m_uSeqStart(other.m_uSeqStart),
    m_uSeqStop(other.m_uSeqStop),
    m_strSource(other.m_strSource),
    m_strType(other.m_strType),
    m_pScore(0),
    m_peStrand(0),
    m_puPhase(0)
{
    m_pLoc = other.m_pLoc;
    if (other.m_pScore) {
        m_pScore = new string(*(other.m_pScore));
    }
    if (other.m_peStrand) {
        m_peStrand = new ENa_strand(*(other.m_peStrand));
    }
    if (other.m_puPhase) {
        m_puPhase = new unsigned int(*(other.m_puPhase));
    }

    this->m_Attributes.insert( 
        other.m_Attributes.begin(), other.m_Attributes.end());
};

//  ----------------------------------------------------------------------------
CGffFeatureRecord::~CGffFeatureRecord()
//  ----------------------------------------------------------------------------
{
    delete m_pScore;
    delete m_peStrand;
    delete m_puPhase; 
};

//  ----------------------------------------------------------------------------
bool CGffFeatureRecord::AddAttribute(
    const string& key,
    const string& value )
//  ----------------------------------------------------------------------------
{
    if (value.empty()) {
        return false; //don't accept blank values 
    }
    TAttrIt it = m_Attributes.find(key);
    if (it == m_Attributes.end()) {
        m_Attributes[key] = vector<string>();
    }
    if (std::find(m_Attributes[key].begin(), m_Attributes[key].end(), value) == 
            m_Attributes[key].end()) {
        m_Attributes[key].push_back(value);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffFeatureRecord::SetAttribute(
    const string& key,
    const string& value )
//  ----------------------------------------------------------------------------
{
    DropAttributes(key);
    return AddAttribute(key, value);
}

//  ----------------------------------------------------------------------------
bool CGffFeatureRecord::GetAttributes(
    const string& key,
    vector<string>& value ) const
//  ----------------------------------------------------------------------------
{
    TAttrCit it = m_Attributes.find(key);
    if (it == m_Attributes.end()  ||  it->second.empty()) {
        return false;
    }
    value = it->second;
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffFeatureRecord::AddAttributes(
    const string& key,
    const vector<string>& values)
//  ----------------------------------------------------------------------------
{
    if (values.empty()) {
        return true; //nothing to do 
    }
    TAttrIt it = m_Attributes.find(key);
    if (it == m_Attributes.end()) {
        m_Attributes[key] = vector<string>(values.begin(), values.end());
        return true;
    }
    for (vector<string>::const_iterator cit = values.begin(); 
            cit != values.end(); ++cit) {
        string current = *cit;
        vector<string>::iterator iit = std::find(
            m_Attributes[key].begin(), m_Attributes[key].end(), current);
        if (iit == m_Attributes[key].end()) {
            m_Attributes[key].push_back(current);
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffFeatureRecord::SetAttributes(
    const string& key,
    const vector<string>& values)
//  ----------------------------------------------------------------------------
{
    m_Attributes[key] = vector<string>(values.begin(), values.end());
    return true;
}

//  ----------------------------------------------------------------------------
string CGffFeatureRecord::StrType() const
//  ----------------------------------------------------------------------------
{
    vector<string> gffType;
    if ( GetAttributes( "gff_type", gffType ) ) {
        return gffType.front();
    }
    return m_strType;
}

//  ----------------------------------------------------------------------------
string CGffFeatureRecord::StrSeqStart() const
//  ----------------------------------------------------------------------------
{
    return NStr::UIntToString( m_uSeqStart + 1 );;
}

//  ----------------------------------------------------------------------------
string CGffFeatureRecord::StrSeqStop() const
//  ----------------------------------------------------------------------------
{
    return NStr::UIntToString( m_uSeqStop + 1 );
}

//  ----------------------------------------------------------------------------
string CGffFeatureRecord::StrScore() const
//  ----------------------------------------------------------------------------
{
    if ( ! m_pScore ) {
        return ".";
    }
    return *m_pScore;
}

//  ----------------------------------------------------------------------------
string CGffFeatureRecord::StrStrand() const
//  ----------------------------------------------------------------------------
{
    if ( ! m_peStrand ) {
        return "+";
    }
    switch ( *m_peStrand ) {
    default:
        return "+";
    case eNa_strand_minus:
        return "-";
    }
}

//  ----------------------------------------------------------------------------
string CGffFeatureRecord::StrPhase() const
//  ----------------------------------------------------------------------------
{
    if ( ! m_puPhase ) {
        return ".";
    }
    return NStr::UIntToString( *m_puPhase );
}

//  ----------------------------------------------------------------------------
string CGffFeatureRecord::StrAttributes() const
//  ----------------------------------------------------------------------------
{
    string strAttributes;
	strAttributes.reserve(256);
    CGffFeatureRecord::TAttributes attrs;
    attrs.insert( Attributes().begin(), Attributes().end() );
    CGffFeatureRecord::TAttrIt it;

    for ( it = attrs.begin(); it != attrs.end(); ++it ) {
        string strKey = it->first;

        if ( ! strAttributes.empty() ) {
            strAttributes += "; ";
        }
        strAttributes += strKey;
        strAttributes += "=";
//        strAttributes += " ";
		
		bool quote = CWriteUtil::NeedsQuoting(it->second.front());
		if ( quote )
			strAttributes += '\"';		
		strAttributes += it->second.front();
		if ( quote )
			strAttributes += '\"';
    }
    if ( strAttributes.empty() ) {
        strAttributes = ".";
    }
    return strAttributes;
}

//  ----------------------------------------------------------------------------
void CGffFeatureRecord::x_StrAttributesAppendValue(
    const string& strKey,
    const string& attr_separator,
    const string& multivalue_separator,
    map<string, vector<string> >& attrs,
    string& strAttributes ) const
//  ----------------------------------------------------------------------------
{
    TAttrIt it = attrs.find( strKey );
    if ( it == attrs.end() ) {
        return;
    }
    string strValue;
    vector<string> tags = it->second;
    for ( vector<string>::iterator pTag = tags.begin(); pTag != tags.end(); pTag++ ) {
        if ( !strValue.empty() ) {
            strValue += multivalue_separator;
        }
        string strTag = CWriteUtil::UrlEncode( *pTag );
        if (CWriteUtil::NeedsQuoting(strTag)) {
            strTag = string("\"") + strTag + string("\"");
        }
        strValue += strTag;
    }

    if ( ! strAttributes.empty() ) {
        strAttributes += attr_separator;
    }
    strAttributes += strKey;
    strAttributes += "=";
    strAttributes += strValue;

	attrs.erase(it);
}

//  ----------------------------------------------------------------------------
void CGffFeatureRecord::SetSeqId(
    const string& seqId) 
//  ----------------------------------------------------------------------------
{
    m_strSeqId = seqId;
}

//  ----------------------------------------------------------------------------
void CGffFeatureRecord::SetFeatureType(
    const string& featureType)
//  ----------------------------------------------------------------------------
{
    m_strType = featureType;
}

//  ----------------------------------------------------------------------------
void CGffFeatureRecord::SetRecordId(
    const string& recordId)
//  ----------------------------------------------------------------------------
{
    SetAttribute("ID", recordId);
}

//  ----------------------------------------------------------------------------
void CGffFeatureRecord::SetSource(
    const string& source)
//  ----------------------------------------------------------------------------
{
    m_strSource = source;
}

//  ----------------------------------------------------------------------------
void CGffFeatureRecord::SetLocation(
    unsigned int seqStart,
    unsigned int seqStop,
    ENa_strand seqStrand)
//  ----------------------------------------------------------------------------
{
    m_uSeqStart = seqStart;
    m_uSeqStop = seqStop;
    if (0 == m_peStrand) {
        m_peStrand = new ENa_strand;
    }
    *m_peStrand = seqStrand;
}

//  ----------------------------------------------------------------------------
void CGffFeatureRecord::SetPhase(
    unsigned int iPhase )
//  ----------------------------------------------------------------------------
{
    if ( 0 == m_puPhase ) {
        m_puPhase = new unsigned int;
    }
    *m_puPhase = (3+iPhase)%3;
}

//  ----------------------------------------------------------------------------
void CGffFeatureRecord::SetStrand(
    ENa_strand strand)
//  ----------------------------------------------------------------------------
{
    if ( 0 == m_peStrand ) {
        m_peStrand = new ENa_strand;
    }
    *m_peStrand = strand;
}

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
        m_uSeqStart = interval.GetFrom();
    }
    if (interval.IsPartialStart(eExtreme_Biological)) {
        string min = NStr::IntToString(m_uSeqStart + 1);
        SetAttribute("start_range", string(".,") + min);
    }
    if ( interval.CanGetTo() ) {
        m_uSeqStop = interval.GetTo();
    }
    if (interval.IsPartialStop(eExtreme_Biological)) {
        string max = NStr::IntToString(m_uSeqStop + 1);
        SetAttribute("end_range", max + string(",."));
    }
    if ( interval.IsSetStrand() ) {
        if ( 0 == m_peStrand ) {
            m_peStrand = new ENa_strand( interval.GetStrand() );
        }
        else {
            *m_peStrand = interval.GetStrand();
        }
    }
}

//  ----------------------------------------------------------------------------
bool CGffFeatureRecord::DropAttributes(
    const string& strAttr )
//  ----------------------------------------------------------------------------
{
    TAttrIt it = m_Attributes.find( strAttr );
    if ( it == m_Attributes.end() ) {
        return false;
    }
    m_Attributes.erase( it );
    return true;
}

END_objects_SCOPE
END_NCBI_SCOPE
