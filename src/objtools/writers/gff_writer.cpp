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

#include <objtools/writers/gff3_write_data.hpp>
#include <objtools/writers/gff_writer.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


//  ---------------------------------------------------------------------------
bool CGff3WriteRecordSet::PGff3WriteRecordPtrLess::operator()( 
	const CGff3WriteRecord* x, 
	const CGff3WriteRecord* y ) const
//  ---------------------------------------------------------------------------
{
	if ( x->Type() != y->Type() )
		return ( x->Type() < y->Type() );
	
	if ( x->Id() != y->Id() )
		return ( x->Id() < y->Id() );
	
    if ( x->SeqStart() != y->SeqStart() )
		return ( x->SeqStart() < y->SeqStart() );
	
	if ( x->SeqStop() != y->SeqStop() )
		return ( x->SeqStop() < y->SeqStop() );

    if ( x->GeneId() != y->GeneId() )
		return ( x->GeneId() < y->GeneId() );
	
    if( x->TranscriptId() != y->TranscriptId() )
		return ( x->TranscriptId() < y->TranscriptId() );

    // equivalent
	return false;
}

//  ---------------------------------------------------------------------------
void CGff3WriteRecordSet::AddRecord(
    CGff3WriteRecord* pRecord ) 
//  ---------------------------------------------------------------------------
{ 
    m_Set.push_back( pRecord ); 
};

//  ---------------------------------------------------------------------------
void CGff3WriteRecordSet::AddOrMergeRecord(
	CGff3WriteRecord* pRecord )
//  ---------------------------------------------------------------------------
{
	TMergeMap::iterator merge = m_MergeMap.find(pRecord);
	if( merge != m_MergeMap.end() ) {
		merge->second->MergeRecord( *pRecord );
		delete pRecord;
	} else {
		m_MergeMap[pRecord] = pRecord;
		m_Set.push_back( pRecord );
	}
}

//  ----------------------------------------------------------------------------
CGffWriter::CGffWriter(
    CNcbiOstream& ostr,
    TFlags uFlags ) :
//  ----------------------------------------------------------------------------
    m_Os( ostr ),
    m_uFlags( uFlags )
{
    m_pObjMngr = CObjectManager::GetInstance();
    m_pScope.Reset( new CScope( *m_pObjMngr ) );
};

//  ----------------------------------------------------------------------------
CGffWriter::~CGffWriter()
//  ----------------------------------------------------------------------------
{
};

//  ----------------------------------------------------------------------------
bool CGffWriter::WriteAnnot( 
    const CSeq_annot& annot )
//  ----------------------------------------------------------------------------
{
    if ( ! (m_uFlags & fNoHeader) ) {
        x_WriteHeader();
    }
    CRef< CUser_object > pBrowserInfo = x_GetDescriptor( annot, "browser" );
    if ( pBrowserInfo ) {
        x_WriteBrowserLine( pBrowserInfo );
    }
    CRef< CUser_object > pTrackInfo = x_GetDescriptor( annot, "track" );
    if ( pTrackInfo ) {
        x_WriteTrackLine( pTrackInfo );
    }

    if ( annot.IsFtable() ) {
        return x_WriteAnnotFTable( annot );
    }
    if ( annot.IsAlign() ) {
        return x_WriteAnnotAlign( annot );
    }
    cerr << "Unexpected!" << endl;
    return false;
}

//  ----------------------------------------------------------------------------
bool CGffWriter::x_WriteAnnotFTable( 
    const CSeq_annot& annot )
//  ----------------------------------------------------------------------------
{
    if ( ! annot.IsFtable() ) {
        return false;
    }

    CGff3WriteRecordSet recordSet;
    CSeq_annot_Handle sah = m_pScope->AddSeq_annot( annot );

    const list< CRef< CSeq_feat > >& table = annot.GetData().GetFtable();
    list< CRef< CSeq_feat > >::const_iterator it = table.begin();
    while ( it != table.end() ) {
        x_AssignObject( sah, **it, recordSet );
        it++;
    }
    m_pScope->RemoveSeq_annot( sah );
    x_WriteRecords( recordSet );
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffWriter::x_WriteAnnotAlign( 
    const CSeq_annot& annot )
//  ----------------------------------------------------------------------------
{
//    cerr << "To be done." << endl;
    return false;
}

//  ----------------------------------------------------------------------------
bool CGffWriter::x_WriteBrowserLine(
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
bool CGffWriter::x_WriteTrackLine(
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
bool CGffWriter::x_AssignObject(
   CSeq_annot_Handle sah,
   const CSeq_feat& feat,        
   CGff3WriteRecordSet& set )
//  ----------------------------------------------------------------------------
{
    CGff3WriteRecord* pRecord = new CGff3WriteRecord( sah );
    if ( ! pRecord->AssignFromAsn( feat ) ) {
        return false;
    }
    if ( pRecord->Type() == "mRNA" ) {
        set.AddRecord( pRecord );
        // create one GFF3 exon feature for each constituent interval of the 
        // mRNA
        const CSeq_loc& loc = feat.GetLocation();
        if ( loc.IsPacked_int() && loc.GetPacked_int().CanGet() ) {
            const list< CRef< CSeq_interval > >& sublocs = loc.GetPacked_int().Get();
            list< CRef< CSeq_interval > >::const_iterator it;
            for ( it = sublocs.begin(); it != sublocs.end(); ++it ) {
                const CSeq_interval& subint = **it;
                CGff3WriteRecord* pExon = new CGff3WriteRecord( sah );
                pExon->MakeExon( *pRecord, subint );
                set.AddOrMergeRecord( pExon );
            }
        }
        return true;
    }

    if ( pRecord->Type() == "exon" ) {     
        set.AddOrMergeRecord( pRecord );
        return true;
    }

    // default behavior:
    set.AddRecord( pRecord );
    return true;
}
    
//  ----------------------------------------------------------------------------
bool CGffWriter::x_WriteRecords(
    const CGff3WriteRecordSet& set )
//  ----------------------------------------------------------------------------
{
    for ( CGff3WriteRecordSet::TCit cit = set.begin(); cit != set.end(); ++cit ) {
        if ( ! x_WriteRecord( **cit ) ) {
            return false;
        }
    }
    return true;
}    
    
//  ----------------------------------------------------------------------------
bool CGffWriter::x_WriteRecord( 
    const CGff3WriteRecord& record )
//  ----------------------------------------------------------------------------
{
    m_Os << x_GffId( record ) << '\t';
    m_Os << x_GffSource( record ) << '\t';
    m_Os << x_GffType( record ) << '\t';
    m_Os << x_GffSeqStart( record ) << '\t';
    m_Os << x_GffSeqStop( record ) << '\t';
    m_Os << x_GffScore( record ) << '\t';
    m_Os << x_GffStrand( record ) << '\t';
    m_Os << x_GffPhase( record ) << '\t';
    m_Os << x_GffAttributes( record ) << endl;
    return true;
}

//  ----------------------------------------------------------------------------
string CGffWriter::x_GffId(
    const CGff3WriteRecord& record ) const
//  ----------------------------------------------------------------------------
{
    return record.Id();
}

//  ----------------------------------------------------------------------------
string CGffWriter::x_GffSource(
    const CGff3WriteRecord& record ) const
//  ----------------------------------------------------------------------------
{
    return record.Source();
}

//  ----------------------------------------------------------------------------
string CGffWriter::x_GffType(
    const CGff3WriteRecord& record ) const
//  ----------------------------------------------------------------------------
{
    string strGffType;
    if ( record.GetAttribute( "gff_type", strGffType ) ) {
        return strGffType;
    }
    return record.Type();
}

//  ----------------------------------------------------------------------------
string CGffWriter::x_GffSeqStart(
    const CGff3WriteRecord& record ) const
//  ----------------------------------------------------------------------------
{
    return NStr::UIntToString( record.SeqStart() + 1 );
}

//  ----------------------------------------------------------------------------
string CGffWriter::x_GffSeqStop(
    const CGff3WriteRecord& record ) const
//  ----------------------------------------------------------------------------
{
    return NStr::UIntToString( record.SeqStop() + 1 );
}

//  ----------------------------------------------------------------------------
string CGffWriter::x_GffScore(
    const CGff3WriteRecord& record ) const
//  ----------------------------------------------------------------------------
{
    if ( ! record.IsSetScore() ) {
        return ".";
    }
    char pcBuffer[ 16 ];
    ::sprintf( pcBuffer, "%6.6f", record.Score() );
    return string( pcBuffer );
}

//  ----------------------------------------------------------------------------
string CGffWriter::x_GffStrand(
    const CGff3WriteRecord& record ) const
//  ----------------------------------------------------------------------------
{
    if ( ! record.IsSetStrand() ) {
        return ".";
    }
    switch ( record.Strand() ) {
    default:
        return ".";
    case eNa_strand_plus:
        return "+";
    case eNa_strand_minus:
        return "-";
    }
}

//  ----------------------------------------------------------------------------
string CGffWriter::x_GffPhase(
    const CGff3WriteRecord& record ) const
//  ----------------------------------------------------------------------------
{
    if ( ! record.IsSetPhase() ) {
        return ".";
    }
    switch ( record.Phase() ) {
    default:
        return "0";
    case CCdregion::eFrame_two:
        return "1";
    case CCdregion::eFrame_three:
        return "2";
    }
}

//  ----------------------------------------------------------------------------
string CGffWriter::x_GffAttributes(
    const CGff3WriteRecord& record ) const
//  ----------------------------------------------------------------------------
{
    string strAttributes;
	strAttributes.reserve(256);
    CGff3WriteRecord::TAttributes attrs;
    attrs.insert( record.Attributes().begin(), record.Attributes().end() );
    CGff3WriteRecord::TAttrIt it;

    if ( ! record.GeneId().empty() ) {
        if ( ! strAttributes.empty() && ! (m_uFlags & fSoQuirks) ) {
            strAttributes += "; ";
        }
        strAttributes += "gene_id=\"";
		strAttributes += record.GeneId();
		strAttributes += "\"";
		if ( m_uFlags & fSoQuirks ) {
            strAttributes += "; ";
        }
    }
    if ( ! record.TranscriptId().empty() ) {
        if ( ! strAttributes.empty() && ! (m_uFlags & fSoQuirks) ) {
            strAttributes += "; ";
        }
        strAttributes += "transcript_id=\"";
		strAttributes += record.TranscriptId();
		strAttributes += "\"";
		if ( m_uFlags & fSoQuirks ) {
            strAttributes += "; ";
        }
    }

    x_PriorityProcess( "ID", attrs, strAttributes );
    x_PriorityProcess( "Name", attrs, strAttributes );
    x_PriorityProcess( "Alias", attrs, strAttributes );
    x_PriorityProcess( "Parent", attrs, strAttributes );
    x_PriorityProcess( "Target", attrs, strAttributes );
    x_PriorityProcess( "Gap", attrs, strAttributes );
    x_PriorityProcess( "Derives_from", attrs, strAttributes );
    x_PriorityProcess( "Note", attrs, strAttributes );
    x_PriorityProcess( "Dbxref", attrs, strAttributes );
    x_PriorityProcess( "Ontology_term", attrs, strAttributes );

    for ( it = attrs.begin(); it != attrs.end(); ++it ) {
        string strKey = it->first;
        if ( NStr::StartsWith( strKey, "gff_" ) ) {
            continue;
        }

        if ( ! strAttributes.empty() && ! (m_uFlags & fSoQuirks) ) {
            strAttributes += "; ";
        }
        strAttributes += strKey;
        strAttributes += "=";
		
		bool quote = x_NeedsQuoting(it->second);
		if ( quote )
			strAttributes += '\"';		
		strAttributes += it->second;
		if ( quote )
			strAttributes += '\"';
		if ( m_uFlags & fSoQuirks ) {
            strAttributes += "; ";
        }
    }
    return strAttributes;
}

//  ----------------------------------------------------------------------------
CRef< CUser_object > CGffWriter::x_GetDescriptor(
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
bool CGffWriter::x_WriteHeader()
//  ----------------------------------------------------------------------------
{
    m_Os << "##gff-version 3" << endl;
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffWriter::x_NeedsQuoting(
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
void CGffWriter::x_PriorityProcess(
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
