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
string CGffWriteRecord::StrId() const
//  ----------------------------------------------------------------------------
{
    return m_strId;
}

//  ----------------------------------------------------------------------------
string CGffWriteRecord::StrSource() const
//  ----------------------------------------------------------------------------
{
    return m_strSource;
}

//  ----------------------------------------------------------------------------
CGffWriteRecord::CGffWriteRecord(
    CGffFeatureContext& fc,
    const string& id ):
    m_fc(fc),
    m_strId( "" ),
    m_uSeqStart( 0 ),
    m_uSeqStop( 0 ),
    m_strSource( "." ),
    m_strType( "." ),
    m_pdScore( 0 ),
    m_peStrand( 0 ),
    m_puPhase( 0 )
//  ----------------------------------------------------------------------------
{
    if (!id.empty()) {
        SetAttribute("ID", id);
    }
};

//  ----------------------------------------------------------------------------
CGffWriteRecord::CGffWriteRecord(
    const CGffWriteRecord& other ):
    m_fc( other.m_fc ),
    m_strId( other.m_strId ),
    m_uSeqStart( other.m_uSeqStart ),
    m_uSeqStop( other.m_uSeqStop ),
    m_strSource( other.m_strSource ),
    m_strType( other.m_strType ),
    m_pdScore( 0 ),
    m_peStrand( 0 ),
    m_puPhase( 0 )
//  ----------------------------------------------------------------------------
{
    if ( other.m_pdScore ) {
        m_pdScore = new double( *(other.m_pdScore) );
    }
    if ( other.m_peStrand ) {
        m_peStrand = new ENa_strand( *(other.m_peStrand) );
    }
    if ( other.m_puPhase ) {
        m_puPhase = new unsigned int( *(other.m_puPhase) );
    }

    this->m_Attributes.insert( 
        other.m_Attributes.begin(), other.m_Attributes.end() );
};

//  ----------------------------------------------------------------------------
CGffWriteRecord::~CGffWriteRecord()
//  ----------------------------------------------------------------------------
{
    delete m_pdScore;
    delete m_peStrand;
    delete m_puPhase; 
};

//  ----------------------------------------------------------------------------
bool CGffWriteRecord::SetAttribute(
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
bool CGffWriteRecord::GetAttribute(
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
string CGffWriteRecord::StrType() const
//  ----------------------------------------------------------------------------
{
    vector<string> gffType;
    if ( GetAttribute( "gff_type", gffType ) ) {
        return gffType.front();
    }
    return m_strType;
}

//  ----------------------------------------------------------------------------
string CGffWriteRecord::StrSeqStart() const
//  ----------------------------------------------------------------------------
{
    return NStr::UIntToString( m_uSeqStart + 1 );;
}

//  ----------------------------------------------------------------------------
string CGffWriteRecord::StrSeqStop() const
//  ----------------------------------------------------------------------------
{
    return NStr::UIntToString( m_uSeqStop + 1 );
}

//  ----------------------------------------------------------------------------
string CGffWriteRecord::StrScore() const
//  ----------------------------------------------------------------------------
{
    if ( ! m_pdScore ) {
        return ".";
    }
    return NStr::DoubleToString( *m_pdScore );
}

//  ----------------------------------------------------------------------------
string CGffWriteRecord::StrStrand() const
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
string CGffWriteRecord::StrPhase() const
//  ----------------------------------------------------------------------------
{
    if ( ! m_puPhase ) {
        return ".";
    }
    return NStr::UIntToString( *m_puPhase );
}

//  ----------------------------------------------------------------------------
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
}

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
        m_uSeqStart = interval.GetFrom();
    }
    if ( interval.CanGetTo() ) {
        m_uSeqStop = interval.GetTo();
    }
    if ( interval.IsSetStrand() ) {
        if ( 0 == m_peStrand ) {
            m_peStrand = new ENa_strand( interval.GetStrand() );
        }
        else {
            *m_peStrand = interval.GetStrand();
        }
    }
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecord::CorrectPhase(
    int iPhase )
//  ----------------------------------------------------------------------------
{
    if ( 0 == m_puPhase ) {
        return false;
    }
    *m_puPhase = (3+iPhase)%3;
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecord::DropAttribute(
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

//  ----------------------------------------------------------------------------
bool CGffWriteRecord::AssignSequenceNumber(
    unsigned int uSequenceNumber,
    const string& strPrefix ) 
//  ----------------------------------------------------------------------------
{
    vector<string> ids;
    if (!GetAttribute("ID", ids)) {
        return false;
    }
    ids.at(0) += string( "|" ) + strPrefix + NStr::UIntToString( uSequenceNumber );
    return false;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecordFeature::AssignFromAsn(
    CMappedFeat mapped_feature,
    unsigned int flags )
//  ----------------------------------------------------------------------------
{
    if ( ! x_AssignType( mapped_feature, flags ) ) {
        return false;
    }
    if ( ! x_AssignSeqId( mapped_feature ) ) {
        return false;
    }
    if ( ! x_AssignSource( mapped_feature ) ) {
        return false;
    }
    if ( ! x_AssignStart( mapped_feature ) ) {
        return false;
    }
    if ( ! x_AssignStop( mapped_feature ) ) {
        return false;
    }
    if ( ! x_AssignScore( mapped_feature ) ) {
        return false;
    }
    if ( ! x_AssignStrand( mapped_feature ) ) {
        return false;
    }
    if ( ! x_AssignPhase( mapped_feature ) ) {
        return false;
    }
    if ( ! x_AssignAttributes( mapped_feature, flags ) ) {
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecordFeature::x_AssignSeqId(
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if (CWriteUtil::GetBestId(mf, m_strId)) {
        return true;
    }
    m_strId = ".";
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecordFeature::x_AssignType(
    CMappedFeat mf,
    unsigned int )
//  ----------------------------------------------------------------------------
{
    m_strType = "region";

    if ( mf.IsSetQual() ) {
        const vector< CRef< CGb_qual > >& quals = mf.GetQual();
        vector< CRef< CGb_qual > >::const_iterator it = quals.begin();
        for( ; it != quals.end(); ++it) {
            if ( !(*it)->CanGetQual() || !(*it)->CanGetVal() ) {
                continue;
            }
            if ( (*it)->GetQual() == "standard_name" ) {
                m_strType = (*it)->GetVal();
                return true;
            }
        }
    }

    switch ( mf.GetFeatSubtype() ) {
    default:
        break;
    case CSeq_feat::TData::eSubtype_cdregion:
        m_strType = "CDS";
        break;
    case CSeq_feat::TData::eSubtype_exon:
        m_strType = "exon";
        break;
    case CSeq_feat::TData::eSubtype_misc_RNA:
        m_strType = "transcript";
        break;
    case CSeq_feat::TData::eSubtype_gene:
        m_strType = "gene";
        break;
    case CSeq_feat::TData::eSubtype_mRNA:
        m_strType = "mRNA";
        break;
    case CSeq_feat::TData::eSubtype_scRNA:
        m_strType = "scRNA";
        break;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecordFeature::x_AssignStart(
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    m_uSeqStart = mf.GetLocation().GetStart( eExtreme_Positional );;
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecordFeature::x_AssignStop(
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    m_uSeqStop = mf.GetLocation().GetStop( eExtreme_Positional );;
    return true;
}

//  ----------------------------------------------------------------------------
CConstRef<CUser_object> sGetUserObjectByType(
    const CUser_object& uo,
    const string& strType )
//  ----------------------------------------------------------------------------
{
    if ( uo.IsSetType() && uo.GetType().IsStr() && 
            uo.GetType().GetStr() == strType ) {
        return CConstRef<CUser_object>( &uo );
    }
    const CUser_object::TData& fields = uo.GetData();
    for ( CUser_object::TData::const_iterator it = fields.begin(); 
            it != fields.end(); 
            ++it ) {
        const CUser_field& field = **it;
        if ( field.IsSetData() ) {
            const CUser_field::TData& data = field.GetData();
            if ( data.Which() == CUser_field::TData::e_Object ) {
                CConstRef<CUser_object> recur = sGetUserObjectByType( 
                    data.GetObject(), strType );
                if ( recur ) {
                    return recur;
                }
            }
        }
    }
    return CConstRef<CUser_object>();
}
    
//  ----------------------------------------------------------------------------
CConstRef<CUser_object> sGetUserObjectByType(
    const list<CRef<CUser_object > >& uos,
    const string& strType)
//  ----------------------------------------------------------------------------
{
    CConstRef<CUser_object> pResult;
    typedef list<CRef<CUser_object > >::const_iterator CIT;
    for (CIT cit=uos.begin(); cit != uos.end(); ++cit) {
        const CUser_object& uo = **cit;
        pResult = sGetUserObjectByType(uo, strType);
        if (pResult) {
            return pResult;
        }
    }
    return CConstRef<CUser_object>();
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecordFeature::x_AssignSource(
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    m_strSource = ".";

    if ( mf.IsSetQual() ) {
        const vector< CRef< CGb_qual > >& quals = mf.GetQual();
        vector< CRef< CGb_qual > >::const_iterator it = quals.begin();
        for ( ; it != quals.end(); ++it ) {
            if ( !(*it)->CanGetQual() || !(*it)->CanGetVal() ) {
                continue;
            }
            if ( (*it)->GetQual() == "gff_source" ) {
                m_strSource = (*it)->GetVal();
                return true;
            }
        }
    }

    if ( mf.IsSetExt() ) {
        CConstRef<CUser_object> model_evidence = sGetUserObjectByType( 
            mf.GetExt(), "ModelEvidence" );
        if ( model_evidence ) {
            string strMethod;
            if ( model_evidence->HasField( "Method" ) ) {
                m_strSource = model_evidence->GetField( 
                    "Method" ).GetData().GetStr();
                    return true;
            }
        }
    }

    if (mf.IsSetExts()) {
        CConstRef<CUser_object> model_evidence = sGetUserObjectByType( 
            mf.GetExts(), "ModelEvidence" );
        if ( model_evidence ) {
            string strMethod;
            if ( model_evidence->HasField( "Method" ) ) {
                m_strSource = model_evidence->GetField( 
                    "Method" ).GetData().GetStr();
                    return true;
            }
        }
    }
        
    CScope& scope = mf.GetScope();
    CSeq_id_Handle idh = sequence::GetIdHandle(mf.GetLocation(),
        &mf.GetScope());
    CWriteUtil::GetIdType(scope.GetBioseqHandle(idh),
        m_strSource);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecordFeature::x_AssignScore(
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if ( !mf.IsSetQual() ) {
        return true;
    }
    const vector< CRef< CGb_qual > >& quals = mf.GetQual();
    vector< CRef< CGb_qual > >::const_iterator it = quals.begin();
    for ( ; it != quals.end(); ++it ) {
        if ( !(*it)->CanGetQual() || !(*it)->CanGetVal() ) {
            continue;
        }
        if ( (*it)->GetQual() == "gff_score" ) {
            m_pdScore = new double( NStr::StringToDouble( (*it)->GetVal() ) );
            return true;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecordFeature::x_AssignStrand(
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    m_peStrand = new ENa_strand( mf.GetLocation().GetStrand() );
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecordFeature::x_AssignPhase(
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if ( mf.GetFeatSubtype() == CSeq_feat::TData::eSubtype_cdregion ) {
        m_puPhase = new unsigned int( 0 ); // will be corrected by external code
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecordFeature::x_AssignAttributes(
    CMappedFeat mapped_feat,
    unsigned int )
//  ----------------------------------------------------------------------------
{
    cerr << "FIXME: CGffWriteRecord::x_AssignAttributes" << endl;
    return true;
}

END_objects_SCOPE
END_NCBI_SCOPE
