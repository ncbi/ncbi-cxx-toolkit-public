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
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/stream_utils.hpp>

#include <util/static_map.hpp>
#include <util/line_reader.hpp>

#include <serial/iterator.hpp>
#include <serial/objistrasn.hpp>

// Objects includes
#include <objects/general/Int_fuzz.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Dbtag.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annot_id.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/Gene_ref.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Trna_ext.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/Variation_ref.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <objtools/readers/reader_exception.hpp>
#include <objtools/readers/line_error.hpp>
#include <objtools/readers/error_container.hpp>
#include <objtools/readers/gff3_sofa.hpp>
#include <objtools/readers/gff2_reader.hpp>
#include <objtools/readers/gff3_reader.hpp>
#include <objtools/readers/gvf_reader.hpp>
//#include <objtools/readers/gvf_data.hpp>
#include <objtools/error_codes.hpp>

#include <algorithm>

#define NCBI_USE_ERRCODE_X   Objtools_Rd_RepMask

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
bool CGvfReadRecord::AssignFromGff(
    const string& strGff )
//  ----------------------------------------------------------------------------
{
    if ( ! CGff3ReadRecord::AssignFromGff( strGff ) ) {
        return false;
    }
    // details go here ...
    return true;
}

//  ----------------------------------------------------------------------------
bool CGvfReadRecord::SanityCheck() const
//  ----------------------------------------------------------------------------
{
    return true;
}

//  ----------------------------------------------------------------------------
CGvfReader::CGvfReader(
    unsigned int uFlags,
    const string& name,
    const string& title ):
//  ----------------------------------------------------------------------------
    CGff3Reader( uFlags, name, title )
{
}

//  ----------------------------------------------------------------------------
CGvfReader::~CGvfReader()
//  ----------------------------------------------------------------------------
{
}

//  ----------------------------------------------------------------------------
bool CGvfReader::x_ParseFeatureGff(
    const string& strLine,
    TAnnots& annots )
//  ----------------------------------------------------------------------------
{
    //
    //  Parse the record and determine which ID the given feature will pertain 
    //  to:
    //
    CGvfReadRecord record;
    if ( ! record.AssignFromGff( strLine ) ) {
        return false;
    }

    CRef<CSeq_annot> pAnnot = x_GetAnnotById( annots, record.Id() );
    return x_MergeRecord( record, pAnnot );
//    return x_UpdateAnnot( record, pAnnot ); 
};

//  ----------------------------------------------------------------------------
CRef<CSeq_annot> CGvfReader::x_GetAnnotById(
    TAnnots& annots,
    const string& strId )
//  ----------------------------------------------------------------------------
{
    for ( TAnnotIt it = annots.begin(); it != annots.end(); ++it ) {
        CSeq_annot& annot = **it;
        if ( ! annot.CanGetId() || annot.GetId().size() != 1 ) {
            // internal error
            return CRef<CSeq_annot>();
        }
    
        CRef< CAnnot_id > pId = *( annot.GetId().begin() );
        if ( ! pId->IsLocal() ) {
            // internal error
            return CRef<CSeq_annot>();
        }
        if ( strId == pId->GetLocal().GetStr() ) {
            return *it;
        }
    }
    CRef<CSeq_annot> pNewAnnot( new CSeq_annot );
    annots.push_back( pNewAnnot );

    CRef< CAnnot_id > pAnnotId( new CAnnot_id );
    pAnnotId->SetLocal().SetStr( strId );
    pNewAnnot->SetId().push_back( pAnnotId );
    pNewAnnot->SetData().SetFtable();

    // if available, add current browser information
    if ( m_CurrentBrowserInfo ) {
        pNewAnnot->SetDesc().Set().push_back( m_CurrentBrowserInfo );
    }

    // if available, add current track information
    if ( m_CurrentTrackInfo ) {
        pNewAnnot->SetDesc().Set().push_back( m_CurrentTrackInfo );
    }

    if ( !m_AnnotName.empty() ) {
        pNewAnnot->AddName(m_AnnotName);
    }
    if ( !m_AnnotTitle.empty() ) {
        pNewAnnot->SetTitle(m_AnnotTitle);
    }

    return pNewAnnot;
}

//  ----------------------------------------------------------------------------
bool CGvfReader::x_MergeRecord(
    const CGvfReadRecord& record,
    CRef< CSeq_annot > pAnnot )
//  ----------------------------------------------------------------------------
{
    if ( ! record.SanityCheck() ) {
        return false;
    }
    CRef< CSeq_feat > pFeature( new CSeq_feat );
    if ( ! x_FeatureSetLocation( record, pFeature ) ) {
        return false;
    }

    CVariation_ref& variation = pFeature->SetData().SetVariation();
    {
        // details go here ...
        variation.SetData().SetNote( "Hello Word!" );
    }
    pAnnot->SetData().SetFtable().push_back( pFeature );
    return true;
}

END_objects_SCOPE
END_NCBI_SCOPE
