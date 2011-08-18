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
    CWriterBase( ostr, uFlags )
{
    m_pScope.Reset( &scope );
};

//  ----------------------------------------------------------------------------
CGff2Writer::CGff2Writer(
    CNcbiOstream& ostr,
    unsigned int uFlags ) :
//  ----------------------------------------------------------------------------
    CWriterBase( ostr, uFlags )
{
    m_pScope.Reset( new CScope( *CObjectManager::GetInstance() ) );
    m_pScope->AddDefaults();
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
    CRef< CUser_object > pBrowserInfo = x_GetDescriptor( annot, "browser" );
    if ( pBrowserInfo ) {
        x_WriteBrowserLine( pBrowserInfo );
    }
    CRef< CUser_object > pTrackInfo = x_GetDescriptor( annot, "track" );
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
    for ( CBioseq_CI bci( seh ); bci; ++bci ) {
        if ( ! x_WriteBioseqHandle( *bci ) ) {
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
    bool bGotSource = false;

    CConstRef<CSeqdesc> pDesc = bsh.GetBioseqCore()->GetClosestDescriptor( 
        CSeqdesc::e_Source );
    if (pDesc ) {
        CGffWriteRecordFeature src_feat;
        src_feat.AssignSource( bsh, *pDesc );
        x_WriteRecord( &src_feat );
    }
    SAnnotSelector sel = x_GetAnnotSelector();
    feature::CFeatTree feat_tree( CFeat_CI( bsh, sel ) );
    for ( CAnnot_CI aci( bsh ); aci; ++aci ) {
        for ( CFeat_CI mf( *aci, sel ); mf; ++mf ) {
            if ( ! x_WriteFeature( feat_tree, *mf ) ) {
                return false;
            }
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
    
    SAnnotSelector sel = x_GetAnnotSelector();
    feature::CFeatTree feat_tree( CFeat_CI( sah, sel) );
    for ( CFeat_CI mf( sah, sel ); mf; ++mf ) {
        if ( ! x_WriteFeature( feat_tree, *mf ) ) {
            return false;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::x_WriteFeature(
    feature::CFeatTree& ftree,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    CRef<CGffWriteRecordFeature> pParent( new CGffWriteRecordFeature );
    if ( ! pParent->AssignFromAsn( mf ) ) {
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
            pChild->CorrectLocation( subint );
            if ( ! x_WriteRecord( pChild ) ) {
                return false;
            }
        }
        return true;
    }
    
    // default behavior:
    return x_WriteRecord( pParent );    
}

//  ----------------------------------------------------------------------------
SAnnotSelector CGff2Writer::x_GetAnnotSelector()
//  ----------------------------------------------------------------------------
{
    SAnnotSelector sel;
    sel.SetSortOrder( SAnnotSelector::eSortOrder_Normal );
    return sel;
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
    CRef< CUser_object > pBrowserInfo = x_GetDescriptor( align, "browser" );
    if ( pBrowserInfo ) {
        x_WriteBrowserLine( pBrowserInfo );
    }
    CRef< CUser_object > pTrackInfo = x_GetDescriptor( align, "track" );
    if ( pTrackInfo ) {
        x_WriteTrackLine( pTrackInfo );
    }
    if ( ! x_WriteAlign( align ) ) {
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::x_WriteAlign( 
    const CSeq_align& align,
    bool )
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
    m_Os << strBrowserLine << endl;   
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
        if ( x_NeedsQuoting( strValue ) ) {
            strValue = string( "\"" ) + strValue + string( "\"" );
        }
        strTrackLine += " ";
        strTrackLine += strKey;
        strTrackLine += "=";
        strTrackLine += strValue;
    } 
    m_Os << strTrackLine << endl;   
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
    m_Os << pRecord->StrAttributes() << endl;
    return true;
}

//  ----------------------------------------------------------------------------
CRef< CUser_object > CGff2Writer::x_GetDescriptor(
    const CSeq_annot& annot,
    const string& strType ) const
//  ----------------------------------------------------------------------------
{
    CRef< CUser_object > pUser;
    if ( ! annot.IsSetDesc() ) {
        return pUser;
    }

    const list< CRef< CAnnotdesc > > descriptors = annot.GetDesc().Get();
    list< CRef< CAnnotdesc > >::const_iterator it;
    for ( it = descriptors.begin(); it != descriptors.end(); ++it ) {
        if ( ! (*it)->IsUser() ) {
            continue;
        }
        const CUser_object& user = (*it)->GetUser();
        if ( user.GetType().GetStr() == strType ) {
            pUser.Reset( new CUser_object );
            pUser->Assign( user );
            return pUser;
        }
    }    
    return pUser;
}

//  ----------------------------------------------------------------------------
CRef< CUser_object > CGff2Writer::x_GetDescriptor(
    const CSeq_align& align,
    const string& strType ) const
//  ----------------------------------------------------------------------------
{
    CRef< CUser_object > pUser;
    return pUser;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::WriteHeader()
//  ----------------------------------------------------------------------------
{
    m_Os << "##gff-version 2" << endl;
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::WriteFooter()
//  ----------------------------------------------------------------------------
{
    m_Os << "###" << endl;
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::x_WriteAssemblyInfo(
    const string& strName,
    const string& strAccession )
//  ----------------------------------------------------------------------------
{
    if ( !strName.empty() ) {
        m_Os << "##assembly name=" << strName << endl;
    }
    if ( !strAccession.empty() ) {
        m_Os << "##assembly accession=" << strAccession << endl;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::x_NeedsQuoting(
    const string& str )
//  ----------------------------------------------------------------------------
{
    if( str.empty() )
		return true;

	for ( size_t u=0; u < str.length(); ++u ) {
        if ( str[u] == '\"' )
			return false;
		if ( str[u] == ' ' || str[u] == ';' ) {
            return true;
        }
    }
    return false;
}

//  ----------------------------------------------------------------------------
void CGff2Writer::x_PriorityProcess(
    const string& strKey,
    map<string, string >& attrs,
    string& strAttributes ) const
//  ----------------------------------------------------------------------------
{
    string strKeyMod( strKey );

    map< string, string >::iterator it = attrs.find( strKeyMod );
    if ( it == attrs.end() ) {
        return;
    }

    // Some of the attributes are multivalue translating into multiple gff attributes
    //  all carrying the same key. These are special cased here:
    //
    if ( strKey == "Dbxref" ) {
        if ( m_uFlags & fSoQuirks ) {
            NStr::ToUpper( strKeyMod );
        }
        vector<string> tags;
        NStr::Tokenize( it->second, ";", tags );
        for ( vector<string>::iterator pTag = tags.begin(); 
            pTag != tags.end(); pTag++ ) {
            if ( ! strAttributes.empty() && ! (m_uFlags & fSoQuirks) ) {
                strAttributes += "; ";
            }
            strAttributes += strKeyMod;
            strAttributes += "=\""; // quoted in all samples I have seen
            strAttributes += *pTag;
            strAttributes += "\"";
		    if ( m_uFlags & fSoQuirks ) {
                strAttributes += "; ";
            }
        }
		attrs.erase(it);
        return;
    }

    // General case: Single value, make straight forward gff attribute:
    //
    if ( ! strAttributes.empty() && ! (m_uFlags & fSoQuirks) ) {
        strAttributes += "; ";
    }
    if ( m_uFlags & fSoQuirks ) {
        NStr::ToUpper( strKeyMod );
    }

    strAttributes += strKeyMod;
    strAttributes += "=";
   	bool quote = x_NeedsQuoting(it->second);
	if ( quote )
		strAttributes += '\"';		
	strAttributes += it->second;
	attrs.erase(it);
	if ( quote )
		strAttributes += '\"';
	if ( m_uFlags & fSoQuirks ) {
        strAttributes += "; ";
    }
}

END_NCBI_SCOPE
