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
#include <objtools/writers/gff2_write_data.hpp>
#include <objmgr/mapped_feat.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
const char* CGffWriteRecord::ATTR_SEPARATOR
//  ----------------------------------------------------------------------------
    = "; ";

//  ----------------------------------------------------------------------------
CGffWriteRecord::CGffWriteRecord(
    CGffFeatureContext& fc,
    const string& id ):
    CGffFeatureRecord(id),
    m_fc(fc)
//  ----------------------------------------------------------------------------
{
    mSeqId = "";
    if (!id.empty()) {
        SetAttribute("ID", id);
    }
};

//  ----------------------------------------------------------------------------
CGffWriteRecord::CGffWriteRecord(
    const CGffWriteRecord& other ): CGffFeatureRecord(other),
    m_fc( other.m_fc )
//  ----------------------------------------------------------------------------
{
    mAttributes.insert(
        other.mAttributes.begin(), other.mAttributes.end() );
};

//  ----------------------------------------------------------------------------
CGffWriteRecord::~CGffWriteRecord()
//  ----------------------------------------------------------------------------
{
};

/*//  ----------------------------------------------------------------------------
string CGffWriteRecord::StrAttributes() const
//  ----------------------------------------------------------------------------
{
    string strAttributes;
    strAttributes.reserve(256);
    CGffWriteRecord::TAttributes attrs;
    attrs.insert( Attributes().begin(), Attributes().end() );
    CGffWriteRecord::TAttrIt it;

    for ( it = attrs.begin(); it != attrs.end(); ++it ) {
        string strKey = it->first;

        if ( ! strAttributes.empty() ) {
            strAttributes += "; ";
        }
        strAttributes += strKey;
        strAttributes += "=";
//        strAttributes += " ";

        bool quote = NeedsQuoting(it->second.front());
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
}*/

//  ----------------------------------------------------------------------------
void CGffWriteRecord::x_StrAttributesAppendValue(
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
        if (NeedsQuoting(strTag)) {
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
bool CGffWriteRecord::CorrectLocation(
    const CGffWriteRecord& parent,
    const CSeq_interval& interval,
    unsigned int seqLength )
//  ----------------------------------------------------------------------------
{
    if ( interval.CanGetFrom() ) {
        mSeqStart = interval.GetFrom();
    }
    if (interval.IsPartialStart(eExtreme_Biological)) {
        DropAttributes("start_range");
        string min = NStr::IntToString(mSeqStart + 1);
        SetAttribute("start_range", string(".,") + min);
    }
    if ( interval.CanGetTo() ) {
        mSeqStop = interval.GetTo();
    }
    if (interval.IsPartialStop(eExtreme_Biological)) {
        DropAttributes("end_range");
        string max = NStr::IntToString(mSeqStop + 1);
        SetAttribute("end_range", max + string(",."));
    }
    if ( interval.IsSetStrand() ) {
        SetStrand(interval.GetStrand());
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecord::CorrectPhase(
    int iPhase )
//  ----------------------------------------------------------------------------
{
    if (mPhase.empty()) {
        return false;
    }
    mPhase = NStr::NumericToString((3+iPhase) % 3);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecord::AssignSequenceNumber(
    unsigned int uSequenceNumber,
    const string& strPrefix )
//  ----------------------------------------------------------------------------
{
    vector<string> ids;
    if (!GetAttributes("ID", ids)) {
        return false;
    }
    ids.at(0) += string( "|" ) + strPrefix + NStr::UIntToString( uSequenceNumber );
    return false;
}

//  ----------------------------------------------------------------------------
bool CGtfFeatureRecord::AssignFromAsn(
    const CMappedFeat& mapped_feature,
    unsigned int flags )
//  ----------------------------------------------------------------------------
{
    if ( ! x_AssignAttributes( mapped_feature, flags ) ) {
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfFeatureRecord::x_AssignAttributes(
    const CMappedFeat& mapped_feat,
    unsigned int )
//  ----------------------------------------------------------------------------
{
    cerr << "FIXME: CGffWriteRecord::x_AssignAttributes" << endl;
    return true;
}

END_objects_SCOPE
END_NCBI_SCOPE
