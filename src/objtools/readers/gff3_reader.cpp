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
#include <objects/seqset/Bioseq_set.hpp>

#include <objtools/readers/reader_exception.hpp>
#include <objtools/readers/line_error.hpp>
#include <objtools/readers/error_container.hpp>
#include <objtools/readers/gff3_sofa.hpp>
#include <objtools/readers/gff2_reader.hpp>
#include <objtools/readers/gff3_reader.hpp>
#include <objtools/readers/gff2_data.hpp>
#include <objtools/error_codes.hpp>

#include <algorithm>

//#include "gff3_data.hpp"

#define NCBI_USE_ERRCODE_X   Objtools_Rd_RepMask

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
string CGff3ReadRecord::x_NormalizedAttributeKey(
    const string& strRawKey )
//  ---------------------------------------------------------------------------
{
    string strKey = CGff2Record::x_NormalizedAttributeKey( strRawKey );
    if ( 0 == NStr::CompareNocase( strRawKey, "ID" ) ) {
        return "ID";
    }
    if ( 0 == NStr::CompareNocase( strKey, "Name" ) ) {
        return "Name";
    }
    if ( 0 == NStr::CompareNocase( strKey, "Alias" ) ) {
        return "Alias";
    }
    if ( 0 == NStr::CompareNocase( strKey, "Parent" ) ) {
        return "Parent";
    }
    if ( 0 == NStr::CompareNocase( strKey, "Target" ) ) {
        return "Target";
    }
    if ( 0 == NStr::CompareNocase( strKey, "Gap" ) ) {
        return "Gap";
    }
    if ( 0 == NStr::CompareNocase( strKey, "Derives_from" ) ) {
        return "Derives_from";
    }
    if ( 0 == NStr::CompareNocase( strKey, "Note" ) ) {
        return "Note";
    }
    if ( 0 == NStr::CompareNocase( strKey, "Dbxref" )  ||
        0 == NStr::CompareNocase( strKey, "Db_xref" ) ) {
        return "Dbxref";
    }
    if ( 0 == NStr::CompareNocase( strKey, "Ontology_term" ) ) {
        return "Ontology_term";
    }
    return strKey;
}

//  ----------------------------------------------------------------------------
CGff3Reader::CGff3Reader(
    unsigned int uFlags,
    const string& name,
    const string& title ):
//  ----------------------------------------------------------------------------
    CGff2Reader( uFlags, name, title )
{
}

//  ----------------------------------------------------------------------------
CGff3Reader::~CGff3Reader()
//  ----------------------------------------------------------------------------
{
}

//  ----------------------------------------------------------------------------
bool CGff3Reader::x_UpdateFeatureCds(
    const CGff2Record& gff,
    CRef<CSeq_feat> pFeature)
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_feat> pAdd = CRef<CSeq_feat>(new CSeq_feat);
    if (!x_FeatureSetLocation(gff, pAdd)) {
        return false;
    }
    pFeature->SetLocation().Add(pAdd->GetLocation());
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Reader::x_UpdateAnnot(
    const CGff2Record& record,
    CRef< CSeq_annot > pAnnot )
//  ----------------------------------------------------------------------------
{
    string gbkey;
    record.GetAttribute("gbkey", gbkey);
    CRef< CSeq_feat > pFeature(new CSeq_feat);

    //  Special case: exon feature belonging to an RNA we have already seen
    if (record.Type() == "exon") {
        string parent;
        if (record.GetAttribute("Parent", parent)) {
            IdToFeatureMap::iterator it = m_MapIdToFeature.find(parent);
            if (it != m_MapIdToFeature.end()) {
                return record.UpdateFeature(m_iFlags, it->second);
            }
        }
    }

    //  Special case: Piece of another feature we have already seen
    string id;
    if (record.GetAttribute("ID", id)) {
        IdToFeatureMap::iterator it = m_MapIdToFeature.find(id);
        if (it != m_MapIdToFeature.end()) {
            return record.UpdateFeature(m_iFlags, it->second);
        }
    }

    //  General case: brand new regular feature
    if (!record.InitializeFeature(m_iFlags, pFeature)) {
        return false;
    }

    string strId;
    if ( record.GetAttribute( "ID", strId ) ) {
        m_MapIdToFeature[ strId ] = pFeature;
    }
    return x_AddFeatureToAnnot( pFeature, pAnnot );
}

END_objects_SCOPE
END_NCBI_SCOPE
