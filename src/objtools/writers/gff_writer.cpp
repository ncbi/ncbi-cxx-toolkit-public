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
 * File Description:  Write gff file
 *
 */

#include <ncbi_pch.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Seq_annot.hpp>
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
#include <objects/seqfeat/BioSource.hpp>

#include <objmgr/feat_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/annot_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/mapped_feat.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>

#include <objtools/writers/write_util.hpp>
#include <objtools/writers/gff3_write_data.hpp>
#include <objtools/writers/gff_writer.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ----------------------------------------------------------------------------
CGff2Writer::CGff2Writer(
    CScope& scope,
    CNcbiOstream& ostr,
    unsigned int uFlags ) :
//  ----------------------------------------------------------------------------
    CWriterBase( ostr, uFlags ),
    m_bHeaderWritten(false)
{
    m_pScope.Reset( &scope );
    GetAnnotSelector();
};

//  ----------------------------------------------------------------------------
CGff2Writer::CGff2Writer(
    CNcbiOstream& ostr,
    unsigned int uFlags ) :
//  ----------------------------------------------------------------------------
    CWriterBase( ostr, uFlags ),
    m_bHeaderWritten(false)
{
    m_pScope.Reset( new CScope( *CObjectManager::GetInstance() ) );
    m_pScope->AddDefaults();
    GetAnnotSelector();
};

//  ----------------------------------------------------------------------------
CGff2Writer::~CGff2Writer()
//  ----------------------------------------------------------------------------
{
};

//  ----------------------------------------------------------------------------
bool CGff2Writer::WriteAnnot( 
    const CSeq_annot& annot,
    const string& strAssemblyName,
    const string& strAssemblyAccession )
//  ----------------------------------------------------------------------------
{
    if ( ! x_WriteAssemblyInfo( strAssemblyName, strAssemblyAccession ) ) {
        return false;
    }
    if ( ! x_WriteAnnot( annot ) ) {
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::x_WriteAnnot( 
    const CSeq_annot& annot )
//  ----------------------------------------------------------------------------
{
    CRef< CUser_object > pBrowserInfo = CWriteUtil::GetDescriptor( 
        annot, "browser" );
    if ( pBrowserInfo ) {
        x_WriteBrowserLine( pBrowserInfo );
    }
    CRef< CUser_object > pTrackInfo = CWriteUtil::GetDescriptor( 
        annot, "track" );
    if ( pTrackInfo ) {
        x_WriteTrackLine( pTrackInfo );
    }
    CSeq_annot_Handle sah = m_pScope->AddSeq_annot( annot );
    bool bWrite = x_WriteSeqAnnotHandle( sah );
    m_pScope->RemoveSeq_annot( sah );
    return bWrite;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::WriteSeqEntryHandle(
    CSeq_entry_Handle seh,
    const string& strAssemblyName,
    const string& strAssemblyAccession )
//  ----------------------------------------------------------------------------
{
    if ( ! x_WriteAssemblyInfo( strAssemblyName, strAssemblyAccession ) ) {
        return false;
    }
    if ( ! x_WriteSeqEntryHandle( seh ) ) {
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::x_WriteSeqEntryHandle(
    CSeq_entry_Handle seh )
//  ----------------------------------------------------------------------------
{
    bool isNucProtSet = ( (seh.IsSet()  &&  seh.GetSet().IsSetClass()  &&  
        seh.GetSet().GetClass() == CBioseq_set::eClass_nuc_prot) );

    if (isNucProtSet) {
        for (CBioseq_CI bci(seh); bci; ++bci) {
            if (bci->IsSetInst_Mol() && (bci->GetInst_Mol() == CSeq_inst::eMol_aa)) {
                continue;
            }
            if (!x_WriteBioseqHandle(*bci)) {
                return false;
            }
        }
        return true;
    }
    if (seh.IsSeq()) {
        return x_WriteBioseqHandle(seh.GetSeq());
    }
    SAnnotSelector sel;
    sel.SetMaxSize(1);
    for (CAnnot_CI aci(seh, sel); aci; ++aci) {
        CFeat_CI fit(*aci, GetAnnotSelector());
        CGffFeatureContext fc(fit, CBioseq_Handle(), *aci);
        CSeq_id_Handle lastId;
        for ( /*0*/; fit; ++fit ) {
            CSeq_id_Handle currentId =fit->GetLocationId();
            if (currentId != lastId) {
                x_WriteSequenceHeader(currentId);
                lastId = currentId;
            }
            if ( ! x_WriteFeature( fc, *fit ) ) {
                return false;
            }
        }
    }

    for (CSeq_entry_CI eci(seh); eci; ++eci) {
        if (!x_WriteSeqEntryHandle(*eci)) {
            return false;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::WriteBioseqHandle(
    CBioseq_Handle bsh,
    const string& strAssemblyName,
    const string& strAssemblyAccession )
//  ----------------------------------------------------------------------------
{
    if ( ! x_WriteAssemblyInfo( strAssemblyName, strAssemblyAccession ) ) {
        return false;
    }
    if ( ! x_WriteBioseqHandle( bsh ) ) {
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::x_WriteBioseqHandle(
    CBioseq_Handle bsh ) 
//  ----------------------------------------------------------------------------
{
    SAnnotSelector sel = GetAnnotSelector();
    CFeat_CI feat_iter(bsh, sel);
    CGffFeatureContext fc(feat_iter, bsh);
    for (;  feat_iter; ++feat_iter) {
        if (!x_WriteFeature(fc, *feat_iter)) {
            return false;
        }
    } 
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::WriteSeqAnnotHandle(
    CSeq_annot_Handle sah,
    const string& strAssemblyName,
    const string& strAssemblyAccession )
//  ----------------------------------------------------------------------------
{
    if ( ! x_WriteAssemblyInfo( strAssemblyName, strAssemblyAccession ) ) {
        return false;
    }
    if ( ! x_WriteSeqAnnotHandle( sah ) ) {
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::x_WriteSeqAnnotHandle(
    CSeq_annot_Handle sah )
//  ----------------------------------------------------------------------------
{
    CConstRef<CSeq_annot> pAnnot = sah.GetCompleteSeq_annot();

    if ( pAnnot->IsAlign() ) {
        for ( CAlign_CI it( sah ); it; ++it ) {
            if ( ! x_WriteAlign( *it ) ) {
                return false;
            }
        }
        return true;
    }

    SAnnotSelector sel = GetAnnotSelector();
    CFeat_CI feat_iter(sah, sel);
    CGffFeatureContext fc(feat_iter, CBioseq_Handle(), sah);
    for ( /*0*/; feat_iter; ++feat_iter ) {
        if ( ! x_WriteFeature( fc, *feat_iter ) ) {
            return false;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::x_WriteFeature(
    CGffFeatureContext& context,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    CRef<CGffWriteRecordFeature> pParent(new CGffWriteRecordFeature(context));
    if ( ! pParent->AssignFromAsn( mf, m_uFlags ) ) {
        return false;
    }

    CRef< CSeq_loc > pPackedInt( new CSeq_loc( CSeq_loc::e_Mix ) );
    pPackedInt->Add( mf.GetLocation() );
    pPackedInt->ChangeToPackedInt();

    if ( pPackedInt->IsPacked_int() && pPackedInt->GetPacked_int().CanGet() ) {
        const list< CRef< CSeq_interval > >& sublocs = pPackedInt->GetPacked_int().Get();
        list< CRef< CSeq_interval > >::const_iterator it;
        for ( it = sublocs.begin(); it != sublocs.end(); ++it ) {
            const CSeq_interval& subint = **it;
            CRef<CGffWriteRecord> pChild( new CGffWriteRecord( *pParent ) );
            pChild->CorrectLocation( *pParent, subint,
                context.BioseqHandle().GetInst().GetLength() );
            if ( ! x_WriteRecord( pChild ) ) {
                return false;
            }
        }
        return true;
    }
    return x_WriteRecord( pParent );    
}

//  ----------------------------------------------------------------------------
SAnnotSelector& CGff2Writer::GetAnnotSelector()
//  ----------------------------------------------------------------------------
{
    if ( !m_Selector.get() ) {
        m_Selector.reset(new SAnnotSelector);
        m_Selector->SetSortOrder(SAnnotSelector::eSortOrder_Normal);
    }
    return *m_Selector;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::WriteAlign( 
    const CSeq_align& align,
    const string& strAssName,
    const string& strAssAcc )
//  ----------------------------------------------------------------------------
{
    if ( ! x_WriteAssemblyInfo( strAssName, strAssAcc ) ) {
        return false;
    }
    if ( ! x_WriteAlign( align ) ) {
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::x_WriteAlign( 
    const CSeq_align& align)
//  ----------------------------------------------------------------------------
{
    return false;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::x_WriteBrowserLine(
    const CRef< CUser_object > pBrowserInfo )
//  ----------------------------------------------------------------------------
{
    string strBrowserLine( "browser" );    
    const vector< CRef< CUser_field > > fields = pBrowserInfo->GetData();
    vector< CRef< CUser_field > >::const_iterator cit;
    for ( cit = fields.begin(); cit != fields.end(); ++cit ) {
        if ( ! (*cit)->CanGetLabel() || ! (*cit)->GetLabel().IsStr() ) {
            continue;
        }
        if ( ! (*cit)->CanGetData() || ! (*cit)->GetData().IsStr() ) {
            continue;
        }
        strBrowserLine += " ";
        strBrowserLine += (*cit)->GetLabel().GetStr();
        strBrowserLine += " ";
        strBrowserLine += (*cit)->GetData().GetStr();
    } 
    m_Os << strBrowserLine << '\n';   
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::x_WriteTrackLine(
    const CRef< CUser_object > pTrackInfo )
//  ----------------------------------------------------------------------------
{
    string strTrackLine( "track" );    
    const vector< CRef< CUser_field > > fields = pTrackInfo->GetData();
    vector< CRef< CUser_field > >::const_iterator cit;
    for ( cit = fields.begin(); cit != fields.end(); ++cit ) {
        if ( ! (*cit)->CanGetLabel() || ! (*cit)->GetLabel().IsStr() ) {
            continue;
        }
        if ( ! (*cit)->CanGetData() || ! (*cit)->GetData().IsStr() ) {
            continue;
        }
        string strKey = (*cit)->GetLabel().GetStr();
        string strValue = (*cit)->GetData().GetStr();
        if ( CWriteUtil::NeedsQuoting( strValue ) ) {
            strValue = string( "\"" ) + strValue + string( "\"" );
        }
        strTrackLine += " ";
        strTrackLine += strKey;
        strTrackLine += "=";
        strTrackLine += strValue;
    } 
    m_Os << strTrackLine << '\n';   
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::x_WriteRecord( 
    const CGffWriteRecord* pRecord )
//  ----------------------------------------------------------------------------
{
    m_Os << pRecord->StrId() << '\t';
    m_Os << pRecord->StrSource() << '\t';
    m_Os << pRecord->StrType() << '\t';
    m_Os << pRecord->StrSeqStart() << '\t';
    m_Os << pRecord->StrSeqStop() << '\t';
    m_Os << pRecord->StrScore() << '\t';
    m_Os << pRecord->StrStrand() << '\t';
    m_Os << pRecord->StrPhase() << '\t';
    m_Os << pRecord->StrAttributes() << '\n';
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::WriteHeader()
//  ----------------------------------------------------------------------------
{
    if (!m_bHeaderWritten) {
        m_Os << "##gff-version 2" << '\n';
        m_bHeaderWritten = true;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::WriteFooter()
//  ----------------------------------------------------------------------------
{
    m_Os << "###" << '\n';
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::x_WriteAssemblyInfo(
    const string& strName,
    const string& strAccession )
//  ----------------------------------------------------------------------------
{
    if ( !strName.empty() ) {
        m_Os << "##assembly name=" << strName << '\n';
    }
    if ( !strAccession.empty() ) {
        m_Os << "##assembly accession=" << strAccession << '\n';
    }
    return true;
}

END_NCBI_SCOPE
