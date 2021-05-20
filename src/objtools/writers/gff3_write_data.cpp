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
#include <corelib/ncbi_safe_static.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/User_object.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/RNA_gen.hpp>
#include <objects/seqfeat/Trna_ext.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seq/so_map.hpp>

#include <objmgr/util/seq_loc_util.hpp>
#include <objmgr/mapped_feat.hpp>
#include <objmgr/util/sequence.hpp>

#include <objtools/writers/write_util.hpp>
#include <objtools/writers/gff3_writer.hpp>
#include <objtools/writers/gff3_write_data.hpp>
#include <objtools/writers/genbank_id_resolve.hpp>

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
const char* CGff3WriteRecordFeature::ATTR_SEPARATOR
//  ----------------------------------------------------------------------------
    = ";";

//  ----------------------------------------------------------------------------
const char* CGff3WriteRecordFeature::MULTIVALUE_SEPARATOR
//  ----------------------------------------------------------------------------
    = ",";

//  ----------------------------------------------------------------------------
string s_MakeGffDbtag(
    const CSeq_id_Handle& idh,
    CScope& scope )
//  ----------------------------------------------------------------------------
{
    CSeq_id_Handle gi_idh = sequence::GetId( idh, scope, sequence::eGetId_ForceGi );
    if ( !gi_idh ) {
        return idh.AsString();
    }
    string strGffTag("GI:");
    gi_idh.GetSeqId()->GetLabel( &strGffTag, CSeq_id::eContent );
    return strGffTag;
}

//  ----------------------------------------------------------------------------
CConstRef<CUser_object> s_GetUserObjectByType(
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
                CConstRef<CUser_object> recur = s_GetUserObjectByType(
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
CGff3WriteRecordFeature::CGff3WriteRecordFeature(
    CGffFeatureContext& fc,
    const string& id )
//  ----------------------------------------------------------------------------
    : CGtfFeatureRecord(fc, id)
{
};

//  ----------------------------------------------------------------------------
CGff3WriteRecordFeature::CGff3WriteRecordFeature(
    const CGff3WriteRecordFeature& other )
//  ----------------------------------------------------------------------------
    : CGtfFeatureRecord( other )
{
};

//  ----------------------------------------------------------------------------
CGff3WriteRecordFeature::~CGff3WriteRecordFeature()
//  ----------------------------------------------------------------------------
{
};

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::AssignFromAsnLinear(
    const CMappedFeat& mf,
    unsigned int flags )
//  ----------------------------------------------------------------------------
{
    return CGtfFeatureRecord::AssignFromAsn(mf, flags);
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::AssignFromAsn(
    const CMappedFeat& mf,
    unsigned int flags )
//  ----------------------------------------------------------------------------
{
    m_pLoc.Reset( new CSeq_loc( CSeq_loc::e_Mix ) );
    m_pLoc->Add( mf.GetLocation() );
    CWriteUtil::ChangeToPackedInt(*m_pLoc);

    CBioseq_Handle bsh = m_fc.BioseqHandle();
    if (!CWriteUtil::IsSequenceCircular(bsh)) {
        return CGtfFeatureRecord::AssignFromAsn(mf, flags);
    }

    //  intervals wrapping around the origin extend beyond the sequence length
    //  instead of breaking and restarting at the origin.
    //
    unsigned int len = bsh.GetInst().GetLength();
    list< CRef< CSeq_interval > >& sublocs = m_pLoc->SetPacked_int().Set();
    if (sublocs.size() < 2) {
        return CGtfFeatureRecord::AssignFromAsn(mf, flags);
    }

    list< CRef< CSeq_interval > >::iterator it, it_ceil=sublocs.end(),
        it_floor=sublocs.end();
    //fl new circular coordinate handling starts here >>>
    for ( it = sublocs.begin(); it != sublocs.end(); ++it ) {
        //fix intervals broken in two for crossing the origin to extend
        //  into virtual space instead
        CSeq_interval& subint = **it;
        if (subint.IsSetFrom()  &&  subint.GetFrom() == 0) {
            it_floor = it;
        }
        if (subint.IsSetTo()  &&  subint.GetTo() == len-1) {
            it_ceil = it;
        }
        if (it_floor != sublocs.end()  &&  it_ceil != sublocs.end()) {
            break;
        }
    }
    if ( it_ceil != sublocs.end()  &&  it_floor != sublocs.end() ) {
        (*it_ceil)->SetTo( (*it_ceil)->GetTo() + (*it_floor)->GetTo() + 1 );
        sublocs.erase(it_floor);
    }

    return CGtfFeatureRecord::AssignFromAsn(mf, flags);
};

//  ----------------------------------------------------------------------------
string CGff3WriteRecordFeature::StrAttributes() const
//  ----------------------------------------------------------------------------
{
    string strAttributes;
    strAttributes.reserve(256);
    CGffWriteRecord::TAttributes attrs;
    attrs.insert(Attributes().begin(), Attributes().end());
    CGffWriteRecord::TAttrIt it;

    x_StrAttributesAppendValueGff3("ID", attrs, strAttributes);
    x_StrAttributesAppendValueGff3("Name", attrs, strAttributes);
    x_StrAttributesAppendValueGff3("Alias", attrs, strAttributes);
    x_StrAttributesAppendValueGff3("Parent", attrs, strAttributes);
    x_StrAttributesAppendValueGff3("Target", attrs, strAttributes);
    x_StrAttributesAppendValueGff3("Gap", attrs, strAttributes);
    x_StrAttributesAppendValueGff3("Derives_from", attrs, strAttributes);
    x_StrAttributesAppendValueGff3("Note", attrs, strAttributes);
    x_StrAttributesAppendValueGff3("Dbxref", attrs, strAttributes);
    x_StrAttributesAppendValueGff3("Ontology_term", attrs, strAttributes);
    x_StrAttributesAppendValueGff3("start_range", attrs, strAttributes);
    x_StrAttributesAppendValueGff3("end_range", attrs, strAttributes);

    while (!attrs.empty()) {
        x_StrAttributesAppendValueGff3(attrs.begin()->first, attrs, strAttributes);
    }
    if (strAttributes.empty()) {
        strAttributes = ".";
    }
    return strAttributes;
}

//  ----------------------------------------------------------------------------
void CGff3WriteRecordFeature::x_StrAttributesAppendValueGff3(
    const string& strKey,
    map<string, vector<string> >& attrs,
    string& strAttributes ) const
//  ----------------------------------------------------------------------------
{
    x_StrAttributesAppendValue( strKey, ATTR_SEPARATOR,
        MULTIVALUE_SEPARATOR, attrs, strAttributes );
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::AssignParent(
    const CGff3WriteRecordFeature& parent )
//  ----------------------------------------------------------------------------
{
    vector<string> parentId;
    if ( !parent.GetAttributes("ID", parentId) ) {
        cerr << "Fix me: Parent record without GFF3 ID tag!" << endl;
        return false;
    }
    DropAttributes("Parent");
    for (vector<string>::iterator it = parentId.begin(); it != parentId.end();
            ++it) {
        SetAttribute("Parent", *it);
    }
    return true;
}

//  ----------------------------------------------------------------------------
void CGff3WriteRecordFeature::ForceAttributeID(
    const string& strId )
//  ----------------------------------------------------------------------------
{
    DropAttributes("ID");
    SetAttribute("ID", strId);
}

END_objects_SCOPE
END_NCBI_SCOPE
