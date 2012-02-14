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
 *   GVF file reader
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/Int_fuzz.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seqfeat/Variation_ref.hpp>
#include <objects/seqfeat/Variation_inst.hpp>

#include <objtools/writers/gff3_write_data.hpp>
#include <objtools/writers/gvf_write_data.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objmgr/mapped_feat.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

int CGvfWriteRecord::s_unique = 0;


//  ----------------------------------------------------------------------------
string CGvfWriteRecord::s_UniqueId()
//  ----------------------------------------------------------------------------
{
    return string( "id_" ) + NStr::IntToString( s_unique++ );
}

//  ----------------------------------------------------------------------------
CGvfWriteRecord::CGvfWriteRecord(
    CGffFeatureContext& fc )
//  ----------------------------------------------------------------------------
    : CGff3WriteRecordFeature( fc )
{
};

//  ----------------------------------------------------------------------------
CGvfWriteRecord::CGvfWriteRecord(
    const CGff3WriteRecordFeature& other )
//  ----------------------------------------------------------------------------
    : CGff3WriteRecordFeature( other )
{
};

//  ----------------------------------------------------------------------------
CGvfWriteRecord::~CGvfWriteRecord()
//  ----------------------------------------------------------------------------
{
};

//  ----------------------------------------------------------------------------
bool CGvfWriteRecord::x_AssignSource(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    m_strSource = ".";
    if ( mapped_feat.IsSetExt() ) {
        const CSeq_feat::TExt& ext = mapped_feat.GetExt();
        if ( ext.IsSetType() && ext.GetType().IsStr() && 
            ext.GetType().GetStr() == "GvfAttributes" ) 
        {
            if ( ext.HasField( "source" ) ) {
                m_strSource = ext.GetField( "source" ).GetData().GetStr();
                return true;
            }
        }
    }

    if ( CSeqFeatData::eSubtype_variation_ref != mapped_feat.GetData().GetSubtype() ) {
        return true;
    }
    const CVariation_ref& variation = mapped_feat.GetData().GetVariation();
    if ( variation.IsSetId() ) {
        m_strSource = variation.GetId().GetDb();
        return true;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGvfWriteRecord::x_AssignType(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    m_strType = ".";
    if ( mapped_feat.IsSetExt() ) {
        const CSeq_feat::TExt& ext = mapped_feat.GetExt();
        if ( ext.IsSetType() && ext.GetType().IsStr() && 
            ext.GetType().GetStr() == "GvfAttributes" ) 
        {
            if ( ext.HasField( "orig-var-type" ) ) {
                m_strType = ext.GetField( "orig-var-type" ).GetData().GetStr();
                return true;
            }
        }
    }

    if ( CSeqFeatData::eSubtype_variation_ref != mapped_feat.GetData().GetSubtype() ) {
        return true;
    }

    const CVariation_ref& var_ref = mapped_feat.GetData().GetVariation();
    if ( var_ref.IsComplex() ) {
        m_strType = "complex_structural_alteration";
        return true;
    }
    if ( var_ref.IsGain() ) {
        m_strType = "copy_number_gain";
        return true;
    }
    if ( var_ref.IsLoss() ) {
        if ( var_ref.IsSetConsequence() ) {
            const CVariation_ref::TConsequence& consequences = var_ref.GetConsequence();
            for ( CVariation_ref::TConsequence::const_iterator cit = consequences.begin();
                cit != consequences.end(); ++cit ) 
            {
                if ( (*cit)->IsLoss_of_heterozygosity() ) {
                    m_strType = "loss_of_heterozygosity";
                    return true;
                }
            } 
        }
        m_strType = "copy_number_loss";
        return true;
    }
    if ( var_ref.IsCNV() ) {
        m_strType = "copy_number_variation";
        return true;
    }

    if ( ! var_ref.GetData().IsInstance() ) {
        return true;
    }
    switch( var_ref.GetData().GetInstance().GetType() ) {
    
    default:
        return true;
    case CVariation_inst::eType_snv:
        m_strType = "single_nucleotide_variation";
        return true;
    }

    return true;
}

//  ----------------------------------------------------------------------------
bool CGvfWriteRecord::x_AssignAttributes(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    if ( ! x_AssignAttributeID( mapped_feat ) ) {
        return false;
    }
    if ( ! x_AssignAttributeParent( mapped_feat ) ) {
        return false;
    }
    if ( ! x_AssignAttributeName( mapped_feat ) ) {
        return false;
    }
    if ( ! x_AssignAttributeVarType( mapped_feat ) ) {
        return false;
    }
    if ( ! x_AssignAttributeStartRange( mapped_feat ) ) {
        return false;
    }
    if ( ! x_AssignAttributeEndRange( mapped_feat ) ) {
        return false;
    }
    if ( ! x_AssignAttributesCustom( mapped_feat ) ) {
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGvfWriteRecord::x_AssignAttributesCustom(
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if ( ! mf.IsSetExt() ) {
        return true;
    }
    const CSeq_feat::TExt& ext = mf.GetExt();
    if ( ! ext.IsSetType() || ! ext.GetType().IsStr() || 
        ext.GetType().GetStr() != "GvfAttributes" ) 
    {
        return true;
    }
    const CUser_object::TData& data = ext.GetData();
    for ( CUser_object::TData::const_iterator cit = data.begin(); 
        cit != data.end(); ++cit )
    {
        string key, value;
        try {
            key = (*cit)->GetLabel().GetStr();
            value = (*cit)->GetData().GetStr();
        }
        catch(...) {
            continue;
        }
        if ( ! NStr::StartsWith( key, "custom-" ) ) {
            continue;
        }
        key = key.substr( string("custom-").length() );
        SetAttribute(key, value);
    } 
    return true;
}

//  ----------------------------------------------------------------------------
bool CGvfWriteRecord::x_AssignAttributeID(
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if ( mf.IsSetExt() ) {
        const CSeq_feat::TExt& ext = mf.GetExt();
        if ( ext.IsSetType() && ext.GetType().IsStr() && 
            ext.GetType().GetStr() == "GvfAttributes" ) 
        {
            if ( ext.HasField( "id" ) ) {
                SetAttribute(
                    "ID", ext.GetField("id").GetData().GetStr());
                return true;
            }
        }
    }

    if ( CSeqFeatData::eSubtype_variation_ref != mf.GetData().GetSubtype() ) {
        SetAttribute("ID", s_UniqueId());
        return true;
    }
    const CVariation_ref& var_ref = mf.GetData().GetVariation();
    if ( ! var_ref.IsSetId() ) {
       SetAttribute("ID", s_UniqueId());
        return true;
    }
    const CVariation_ref::TId& id = var_ref.GetId();
    string strId;
    id.GetLabel( &strId );
    SetAttribute("ID", strId);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGvfWriteRecord::x_AssignAttributeParent(
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if ( mf.IsSetExt() ) {
        const CSeq_feat::TExt& ext = mf.GetExt();
        if ( ext.IsSetType() && ext.GetType().IsStr() && 
            ext.GetType().GetStr() == "GvfAttributes" ) 
        {
            if ( ext.HasField( "parent" ) ) {
                SetAttribute(
                    "Parent", ext.GetField("parent").GetData().GetStr());
                return true;
            }
        }
    }

    if ( CSeqFeatData::eSubtype_variation_ref != mf.GetData().GetSubtype() ) {
        return true;
    }
    const CVariation_ref& var_ref = mf.GetData().GetVariation();
    if ( ! var_ref.IsSetParent_id() ) {
        return true;
    }
    const CVariation_ref::TId& id = var_ref.GetParent_id();
    string strId;
    id.GetLabel( &strId );
    SetAttribute("Parent", strId);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGvfWriteRecord::x_AssignAttributeName(
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if ( CSeqFeatData::eSubtype_variation_ref != mf.GetData().GetSubtype() ) {
        return true;
    }
    const CVariation_ref& var_ref = mf.GetData().GetVariation();
    if ( ! var_ref.IsSetName() ) {
        return true;
    }
    SetAttribute("Name", var_ref.GetName());
    return true;
}

//  ----------------------------------------------------------------------------
bool CGvfWriteRecord::x_AssignAttributeVarType(
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if ( mf.IsSetExt() ) {
        const CSeq_feat::TExt& ext = mf.GetExt();
        if ( ext.IsSetType() && ext.GetType().IsStr() && 
            ext.GetType().GetStr() == "GvfAttributes" ) 
        {
            if ( ext.HasField( "custom-var_type" ) ) {
                SetAttribute("var_type", 
                    ext.GetField( "custom-var_type" ).GetData().GetStr());
                return true;
            }
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGvfWriteRecord::x_AssignAttributeStartRange(
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    const CSeq_loc& loc = mf.GetLocation();
    if ( ! loc.IsInt() ) {
        return true;
    }
    const CSeq_interval& intv = loc.GetInt();
    if ( ! intv.IsSetFuzz_from() ) {
        return true;
    }
    const CSeq_interval::TFuzz_from& fuzz = intv.GetFuzz_from();
    
    switch( fuzz.Which() ) {
    
        default:
            return true;

        case CInt_fuzz::e_Range: {
            int min = fuzz.GetRange().GetMin() + 1;
            int max = fuzz.GetRange().GetMax() + 1;
            SetAttribute(
                "Start_range", NStr::IntToString(min) + "," +
                NStr::IntToString(max));
            return true;
        }
        case CInt_fuzz::e_Lim: {
            string min = NStr::IntToString( intv.GetFrom() + 1 );
            if ( fuzz.GetLim() == CInt_fuzz::eLim_gt ) {
                SetAttribute(
                    "Start_range", min + string(",."));
            }
            else if ( fuzz.GetLim() == CInt_fuzz::eLim_lt ) {
                SetAttribute(
                    "Start_range", string(".,") + min);
            }
            return true;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGvfWriteRecord::x_AssignAttributeEndRange(
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    const CSeq_loc& loc = mf.GetLocation();
    if ( ! loc.IsInt() ) {
        return true;
    }
    const CSeq_interval& intv = loc.GetInt();
    if ( ! intv.IsSetFuzz_to() ) {
        return true;
    }
    const CSeq_interval::TFuzz_to& fuzz = intv.GetFuzz_to();
    
    switch( fuzz.Which() ) {
    
        default:
            return true;

        case CInt_fuzz::e_Range: {
            int min = fuzz.GetRange().GetMin() + 1;
            int max = fuzz.GetRange().GetMax() + 1;
            SetAttribute(
                "End_range", NStr::IntToString( min ) + "," +
                NStr::IntToString(max));
            return true;
        }
        case CInt_fuzz::e_Lim: {
            string max = NStr::IntToString( intv.GetTo() + 1 );
            if ( fuzz.GetLim() == CInt_fuzz::eLim_gt ) {
                SetAttribute("End_range", max + string(",."));
            }
            else if ( fuzz.GetLim() == CInt_fuzz::eLim_lt ) {
                SetAttribute("End_range", string(".,") + max);
            }
            return true;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
string CGvfWriteRecord::StrAttributes() const
//  ----------------------------------------------------------------------------
{
    TAttributes temp_attrs( m_Attributes.begin(), m_Attributes.end() );
    string strAttributes;

    TAttrIt priority = temp_attrs.find("ID");
    if ( priority != temp_attrs.end() ) {
        x_AppendAttribute( priority, strAttributes );
        temp_attrs.erase( priority );
    }
    priority = temp_attrs.find("Parent");
    if ( priority != temp_attrs.end() ) {
        x_AppendAttribute( priority, strAttributes );
        temp_attrs.erase( priority );
    }
    priority = temp_attrs.find("Name");
    if ( priority != temp_attrs.end() ) {
        x_AppendAttribute( priority, strAttributes );
        temp_attrs.erase( priority );
    }
    priority = temp_attrs.find("Start_range");
    if ( priority != temp_attrs.end() ) {
        x_AppendAttribute( priority, strAttributes );
        temp_attrs.erase( priority );
    }
    priority = temp_attrs.find("End_range");
    if ( priority != temp_attrs.end() ) {
        x_AppendAttribute( priority, strAttributes );
        temp_attrs.erase( priority );
    }
    TAttrIt other = temp_attrs.begin();
    while ( other != temp_attrs.end() ) {
        x_AppendAttribute( other, strAttributes );
        other++;
    }
    return strAttributes;
}

//  ----------------------------------------------------------------------------
void CGvfWriteRecord::x_AppendAttribute(
    TAttrCit it,
    string& strAttributes ) const
//  ----------------------------------------------------------------------------
{
    string key = it->first;
    string value = it->second.front();
    bool needsQuotes = ( NStr::Find( value, " " ) != NPOS );

    if ( !strAttributes.empty() ) {
        strAttributes += ";";
    }
    strAttributes += key;
    strAttributes += "=";
    if (needsQuotes) {
        strAttributes += "\"";
    }
    strAttributes += value;
    if (needsQuotes) {
        strAttributes += "\"";
    }
}
    
END_objects_SCOPE
END_NCBI_SCOPE
