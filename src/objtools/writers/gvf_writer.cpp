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
 * Authors:  Frank Ludwig
 *
 * File Description:  Write gvf file
 *
 */

#include <ncbi_pch.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annot_id.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seqfeat/Seq_feat.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Score.hpp>

#include <objmgr/feat_ci.hpp>
#include <objmgr/mapped_feat.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>

#include <objtools/writers/gvf_write_data.hpp>
#include <objtools/writers/gvf_writer.hpp>
#include <objtools/alnmgr/alnmap.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ----------------------------------------------------------------------------
bool s_ExtractPragma(
    const CUser_object& pragmas,
    const string& key,
    string& pragma )
//  ----------------------------------------------------------------------------
{
    if ( ! pragmas.HasField( key ) ) {
        return false;
    }
    try {
        pragma = pragmas.GetField( key ).GetData().GetStr();
        return true;
    }
    catch( ... ) {};
    return false;
}

//  ----------------------------------------------------------------------------
CGvfWriter::CGvfWriter(
    CScope& scope,
    CNcbiOstream& ostr,
    unsigned int uFlags ) :
//  ----------------------------------------------------------------------------
    CGff3Writer( scope, ostr, uFlags )
{
    m_uRecordId = 1;
    m_uPendingGeneId = 0;
    m_uPendingMrnaId = 0;
};

//  ----------------------------------------------------------------------------
CGvfWriter::CGvfWriter(
    CNcbiOstream& ostr,
    unsigned int uFlags ) :
//  ----------------------------------------------------------------------------
    CGff3Writer( ostr, uFlags )
{
    m_uRecordId = 1;
    m_uPendingGeneId = 0;
    m_uPendingMrnaId = 0;
};

//  ----------------------------------------------------------------------------
CGvfWriter::~CGvfWriter()
//  ----------------------------------------------------------------------------
{
};

//  ----------------------------------------------------------------------------
bool CGvfWriter::WriteHeader(
    const CSeq_annot& annot )
//  ----------------------------------------------------------------------------
{
    if (!annot.IsSetDesc()  ||  !annot.GetDesc().IsSet()) {
        return WriteHeader();
    }
    const list< CRef< CAnnotdesc > > descrs = annot.GetDesc().Get();
    list< CRef< CAnnotdesc > >::const_iterator cit = descrs.begin();
    CConstRef<CAnnotdesc> pDescPragmas;
    while ( cit != descrs.end() ) {
        CConstRef<CAnnotdesc> pDesc = *cit;
        cit++;
        if ( ! pDesc->IsUser() ) {
            continue;
        }
        if ( ! pDesc->GetUser().IsSetType() ) {
            continue;
        }
        if ( ! pDesc->GetUser().GetType().IsStr() ) {
            continue;
        }
        if ( pDesc->GetUser().GetType().GetStr() == "gvf-import-pragmas" ) {
            pDescPragmas = pDesc;
            break;
        }
    }

    if ( ! WriteHeader() ) {
        return false;
    } 
    if ( ! pDescPragmas ) {
        return true;
    }

    const CAnnotdesc::TUser& pragmas = pDescPragmas->GetUser();
    const CUser_object::TData& data = pragmas.GetData();
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
        if ( key == "gff-version" || key == "gvf-version" ) {
            continue;
        }
        m_Os << "##" << key << " " << value << '\n';
    } 
    return true;
}

//  ----------------------------------------------------------------------------
bool CGvfWriter::WriteHeader()
//  ----------------------------------------------------------------------------
{
    if (!m_bHeaderWritten) {
        m_Os << "##gff-version 3" << '\n';
        m_Os << "##gvf-version 1.05" << '\n';
        m_bHeaderWritten = true;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGvfWriter::xWriteFeature(
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    switch( mf.GetFeatSubtype() ) {

    default:
        return true;

    case CSeqFeatData::eSubtype_variation_ref:
        return xWriteFeatureVariationRef( fc, mf );
    }
}

//  ----------------------------------------------------------------------------
bool CGvfWriter::xWriteFeatureVariationRef(
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    CRef<CGvfWriteRecord> pRecord( new CGvfWriteRecord( fc ) );

    if ( ! pRecord->AssignFromAsn( mf ) ) {
        return false;
    }
    return x_WriteRecord( pRecord );
}

END_NCBI_SCOPE
