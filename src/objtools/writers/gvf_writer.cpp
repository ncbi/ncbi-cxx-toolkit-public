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

#include <objects/seq/Annot_descr.hpp>
#include <objects/general/User_object.hpp>

#include <objtools/writers/gvf_write_data.hpp>
#include <objtools/writers/gvf_writer.hpp>

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
    CFeat_CI feat_it)
//  ----------------------------------------------------------------------------
{
    if (!feat_it) {
        return false;
    }
    CGffFeatureContext fc(feat_it, CBioseq_Handle(), feat_it.GetAnnot());
    return xWriteFeature(fc, *feat_it);
}

//  ----------------------------------------------------------------------------
bool CGvfWriter::xWriteFeature(
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    CGffFeatureContext dummy_fc;

    switch( mf.GetFeatSubtype() ) {
    default:
        return true;
    case CSeqFeatData::eSubtype_variation_ref:
        return xWriteFeatureVariationRef( dummy_fc, mf );
    }
}

//  ----------------------------------------------------------------------------
bool CGvfWriter::xWriteFeatureVariationRef(
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    if (IsCanceled()) {
        NCBI_THROW(
            CObjWriterException,
            eInterrupted,
            "Processing terminated by user");
    }

    CRef<CGvfWriteRecord> pRecord( new CGvfWriteRecord( fc ) );

    if (!xAssignFeature(*pRecord, fc, mf)) {
        return false;
    }
    if ( ! pRecord->AssignFromAsn( mf ) ) {
        return false;
    }
    return xWriteRecord(*pRecord);
}

//  ----------------------------------------------------------------------------
bool CGvfWriter::xWriteRecord(
    const CGffBaseRecord& pRecord )
//  ----------------------------------------------------------------------------
{
    m_Os << pRecord.StrSeqId() << '\t';
    m_Os << pRecord.StrMethod() << '\t';
    m_Os << pRecord.StrType() << '\t';
    m_Os << pRecord.StrSeqStart() << '\t';
    m_Os << pRecord.StrSeqStop() << '\t';
    m_Os << pRecord.StrScore() << '\t';
    m_Os << pRecord.StrStrand() << '\t';
    m_Os << pRecord.StrPhase() << '\t';
    m_Os << pRecord.StrAttributes() << '\n';
    return true;
}

END_NCBI_SCOPE
