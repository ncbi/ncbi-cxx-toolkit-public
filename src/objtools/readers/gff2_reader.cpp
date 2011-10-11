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
#include <objects/seq/Seq_inst.hpp>
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
#include <objtools/readers/gff2_data.hpp>
#include <objtools/error_codes.hpp>

#include <algorithm>

//#include "gff3_data.hpp"

#define NCBI_USE_ERRCODE_X   Objtools_Rd_RepMask

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
bool CGff2Reader::s_GetAnnotId(
    const CSeq_annot& annot,
    string& strId )
//  ----------------------------------------------------------------------------
{
    if ( ! annot.CanGetId() || annot.GetId().size() != 1 ) {
        // internal error
        return false;
    }
    
    CRef< CAnnot_id > pId = *( annot.GetId().begin() );
    if ( ! pId->IsLocal() ) {
        // internal error
        return false;
    }
    strId = pId->GetLocal().GetStr();
    return true;
}

//  ----------------------------------------------------------------------------
CGff2Reader::CGff2Reader(
    int iFlags,
    const string& name,
    const string& title ):
//  ----------------------------------------------------------------------------
    CReaderBase( iFlags ),
    m_pErrors( 0 ),
    m_AnnotName( name ),
    m_AnnotTitle( title )
{
}

//  ----------------------------------------------------------------------------
CGff2Reader::~CGff2Reader()
//  ----------------------------------------------------------------------------
{
}


//  --------------------------------------------------------------------------- 
void
CGff2Reader::ReadSeqAnnots(
    vector< CRef<CSeq_annot> >& annots,
    CNcbiIstream& istr,
    IErrorContainer* pErrorContainer )
//  ---------------------------------------------------------------------------
{
    CStreamLineReader lr( istr );
    ReadSeqAnnots( annots, lr, pErrorContainer );
}

//  ---------------------------------------------------------------------------                       
void
CGff2Reader::ReadSeqAnnots(
    vector< CRef<CSeq_annot> >& annots,
    ILineReader& lr,
    IErrorContainer* pErrorContainer )
//  ----------------------------------------------------------------------------
{
    if ( m_iFlags & fNewCode ) {
        return ReadSeqAnnotsNew( annots, lr, pErrorContainer );
    }
    CRef< CSeq_entry > entry = ReadSeqEntry( lr, pErrorContainer );
    CTypeIterator<CSeq_annot> annot_iter( *entry );
    for ( ;  annot_iter;  ++annot_iter) {
        annots.push_back( CRef<CSeq_annot>( annot_iter.operator->() ) );
    }
}
 
//  ---------------------------------------------------------------------------                       
void
CGff2Reader::ReadSeqAnnotsNew(
    vector< CRef<CSeq_annot> >& annots,
    ILineReader& lr,
    IErrorContainer* pErrorContainer )
//  ----------------------------------------------------------------------------
{
    string line;
    int linecount = 0;

    while ( ! lr.AtEOF() ) {
        ++linecount;
        line = NStr::TruncateSpaces( *++lr );
        if ( NStr::TruncateSpaces( line ).empty() ) {
            continue;
        }
        try {
            if ( x_IsCommentLine( line ) ) {
                continue;
            }
            if ( x_ParseStructuredCommentGff( line, m_CurrentTrackInfo ) ) {
                continue;
            }
            if ( x_ParseBrowserLineGff( line, m_CurrentBrowserInfo ) ) {
                continue;
            }
            if ( x_ParseTrackLineGff( line, m_CurrentTrackInfo ) ) {
                continue;
            }
            if ( ! x_ParseFeatureGff( line, annots ) ) {
                continue;
            }
        }
        catch( CObjReaderLineException& err ) {
            err.SetLineNumber( linecount );
        }
    }
    x_AddConversionInfoGff( annots, &m_ErrorsPrivate );
}

//  ----------------------------------------------------------------------------                
CRef< CSeq_entry >
CGff2Reader::ReadSeqEntry(
    ILineReader& lr,
    IErrorContainer* pErrorContainer ) 
//  ----------------------------------------------------------------------------                
{ 
    vector<CRef<CSeq_annot> > annots;
    ReadSeqAnnotsNew( annots, lr, pErrorContainer );
    
    CRef<CSeq_entry> pSeqEntry(new CSeq_entry());
    pSeqEntry->SetSet();

    for (vector<CRef<CSeq_annot> >::iterator it = annots.begin(); 
            it != annots.end(); ++it) {
        CRef<CBioseq> pSeq( new CBioseq() );
        pSeq->SetAnnot().push_back(*it);
        pSeq->SetId().push_back( CRef<CSeq_id>( 
            new CSeq_id(CSeq_id::e_Local, "gff-import") ) );
        pSeq->SetInst().SetRepr(CSeq_inst::eRepr_not_set);
        pSeq->SetInst().SetMol(CSeq_inst::eMol_not_set);

        CRef<CSeq_entry> pEntry(new CSeq_entry());
        pEntry->SetSeq(*pSeq);
        pSeqEntry->SetSet().SetSeq_set().push_back( pEntry );
    }
    return pSeqEntry;
}

//  ----------------------------------------------------------------------------                
CRef< CSerialObject >
CGff2Reader::ReadObject(
    ILineReader& lr,
    IErrorContainer* pErrorContainer ) 
//  ----------------------------------------------------------------------------                
{ 
    CRef<CSerialObject> object( 
        ReadSeqEntry( lr, pErrorContainer ).ReleaseOrNull() );
    return object;
}
    
//  ----------------------------------------------------------------------------                
void 
CGff2Reader::x_Info(
    const string& message,
    unsigned int line )
//  ----------------------------------------------------------------------------                
{
    if ( !m_pErrors ) {
        return x_Info( message, line );
    }
    CObjReaderLineException err( eDiag_Info, line, message );
    CReaderBase::m_uLineNumber = line;
    ProcessError( err, m_pErrors );
}

//  ----------------------------------------------------------------------------                
void 
CGff2Reader::x_Warn(
    const string& message,
    unsigned int line )
//  ----------------------------------------------------------------------------                
{
    if ( !m_pErrors ) {
        return x_Warn( message, line );
    }
    CObjReaderLineException err( eDiag_Warning, line, message );
    CReaderBase::m_uLineNumber = line;
    ProcessError( err, m_pErrors );
}

//  ----------------------------------------------------------------------------                
void 
CGff2Reader::x_Error(
    const string& message,
    unsigned int line )
//  ----------------------------------------------------------------------------                
{
    if ( !m_pErrors ) {
        return x_Error( message, line );
    }
    CObjReaderLineException err( eDiag_Error, line, message );
    CReaderBase::m_uLineNumber = line;
    ProcessError( err, m_pErrors );
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::x_ReadLine(
    ILineReader& lr,
    string& strLine )
//  ----------------------------------------------------------------------------
{
    strLine.clear();
    while ( ! lr.AtEOF() ) {
        strLine = NStr::TruncateSpaces( *++lr );
        ++m_uLineNumber;
        NStr::TruncateSpacesInPlace( strLine );
        if ( ! x_IsCommentLine( strLine ) ) {
            return true;
        }
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::x_IsCommentLine(
    const string& strLine )
//  ----------------------------------------------------------------------------
{
    if ( strLine.empty() ) {
        return true;
    }
    return (strLine[0] == '#' && strLine[1] != '#');
}

//  ----------------------------------------------------------------------------
void CGff2Reader::x_SetTrackDataToSeqEntry(
    CRef<CSeq_entry>& entry,
    CRef<CUser_object>& trackdata,
    const string& strKey,
    const string& strValue )
//  ----------------------------------------------------------------------------
{
    CSeq_descr& descr = entry->SetDescr();

    if ( strKey == "name" ) {
        CRef<CSeqdesc> name( new CSeqdesc() );
        name->SetName( strValue );
        descr.Set().push_back( name );
        return;
    }
    if ( strKey == "description" ) {
        CRef<CSeqdesc> title( new CSeqdesc() );
        title->SetTitle( strValue );
        descr.Set().push_back( title );
        return;
    }
    trackdata->AddField( strKey, strValue );
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::x_ParseStructuredCommentGff(
    const string& strLine,
    CRef< CAnnotdesc >& )
//  ----------------------------------------------------------------------------
{
    if ( ! NStr::StartsWith( strLine, "##" ) ) {
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::x_ParseFeatureGff(
    const string& strLine,
    TAnnots& annots )
//  ----------------------------------------------------------------------------
{
    //
    //  Parse the record and determine which ID the given feature will pertain 
    //  to:
    //
    CGff2Record* pRecord = x_CreateRecord();
    if ( ! pRecord->AssignFromGff( strLine ) ) {
        return false;
    }

    //
    //  Search annots for a pre-existing annot pertaining to the same ID:
    //
    TAnnotIt it = annots.begin();
    for ( /*NOOP*/; it != annots.end(); ++it ) {
        string strAnnotId;
        if ( ! s_GetAnnotId( **it, strAnnotId ) ) {
            return false;
        }
        if ( pRecord->Id() == strAnnotId ) {
            break;
        }
    }

    //
    //  If a preexisting annot was found, update it with the new feature
    //  information:
    //
    if ( it != annots.end() ) {
        if ( ! x_UpdateAnnot( *pRecord, *it ) ) {
            return false;
        }
    }

    //
    //  Otherwise, create a new annot pertaining to the new ID and initialize it
    //  with the given feature information:
    //
    else {
        CRef< CSeq_annot > pAnnot( new CSeq_annot );
        if ( ! x_InitAnnot( *pRecord, pAnnot ) ) {
            return false;
        }
        annots.push_back( pAnnot );      
    }
 
    delete pRecord;
    return true; 
};

//  ----------------------------------------------------------------------------
bool CGff2Reader::x_ParseBrowserLineGff(
    const string& strRawInput,
    CRef< CAnnotdesc >& pAnnotDesc )
//  ----------------------------------------------------------------------------
{ 
    if ( ! NStr::StartsWith( strRawInput, "browser" ) ) {
        return false;
    }
    vector< string > columns;
    NStr::Tokenize( strRawInput, " \t", columns, NStr::eMergeDelims );

    if ( columns.size() <= 1 || 1 != ( columns.size() % 2 ) ) {
        // don't know how to unwrap this
        pAnnotDesc.Reset();
        return true;
    }    
    pAnnotDesc.Reset( new CAnnotdesc );
    CUser_object& user = pAnnotDesc->SetUser();
    user.SetType().SetStr( "browser" );

    for ( size_t u = 1 /* skip "browser" */; u < columns.size(); u += 2 ) {
        user.AddField( columns[ u ], columns[ u+1 ] );
    }
    return true; 
};

//  ----------------------------------------------------------------------------
bool CGff2Reader::x_ParseTrackLineGff(
    const string& strRawInput,
    CRef< CAnnotdesc >& pAnnotDesc )
//  ----------------------------------------------------------------------------
{ 
    const char cBlankReplace( '+' );

    if ( ! NStr::StartsWith( strRawInput, "track" ) ) {
        return false;
    }

    string strCookedInput( strRawInput );
    bool bInString = false;
    for ( size_t u=0; u < strCookedInput.length(); ++u ) {
        if ( strCookedInput[u] == ' ' && bInString ) {
            strCookedInput[u] = cBlankReplace;
        }
        if ( strCookedInput[u] == '\"' ) {
            bInString = !bInString;
        }
    }
    vector< string > columns;
    NStr::Tokenize( strCookedInput, " \t", columns, NStr::eMergeDelims );

    if ( columns.size() <= 1 ) {
        pAnnotDesc.Reset();
        return true;
    } 
    pAnnotDesc.Reset( new CAnnotdesc );
    CUser_object& user = pAnnotDesc->SetUser();
    user.SetType().SetStr( "track" );

    for ( size_t u = 1 /* skip "track" */; u < columns.size(); ++u ) {
        string strKey;
        string strValue;
        NStr::SplitInTwo( columns[u], "=", strKey, strValue );
        NStr::TruncateSpacesInPlace( strKey, NStr::eTrunc_End );
        if ( NStr::StartsWith( strValue, "\"" ) && NStr::EndsWith( strValue, "\"" ) ) {
            strValue = strValue.substr( 1, strValue.length() - 2 );
        }
        for ( unsigned u = 0; u < strValue.length(); ++u ) {
            if ( strValue[u] == cBlankReplace ) {
                strValue[u] = ' ';
            }
        } 
        NStr::TruncateSpacesInPlace( strValue, NStr::eTrunc_Begin );
        user.AddField( strKey, strValue );
    }
       
    return true; 
};
                                
//  ----------------------------------------------------------------------------
void CGff2Reader::x_AddConversionInfoGff(
    TAnnots&,
    IErrorContainer* )
//  ----------------------------------------------------------------------------
{
}                    

//  ----------------------------------------------------------------------------
bool CGff2Reader::x_InitAnnot(
    const CGff2Record& gff,
    CRef< CSeq_annot > pAnnot )
//  ----------------------------------------------------------------------------
{
    CRef< CAnnot_id > pAnnotId( new CAnnot_id );
    pAnnotId->SetLocal().SetStr( gff.Id() );
    pAnnot->SetId().push_back( pAnnotId );
    pAnnot->SetData().SetFtable();

    // if available, add current browser information
    if ( m_CurrentBrowserInfo ) {
        pAnnot->SetDesc().Set().push_back( m_CurrentBrowserInfo );
    }

    // if available, add current track information
    if ( m_CurrentTrackInfo ) {
        pAnnot->SetDesc().Set().push_back( m_CurrentTrackInfo );
    }

    if ( !m_AnnotName.empty() ) {
        pAnnot->AddName(m_AnnotName);
    }
    if ( !m_AnnotTitle.empty() ) {
        pAnnot->SetTitle(m_AnnotTitle);
    }

    return x_UpdateAnnot( gff, pAnnot );
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::x_UpdateAnnot(
    const CGff2Record& gff,
    CRef< CSeq_annot > pAnnot )
//  ----------------------------------------------------------------------------
{
    CRef< CSeq_feat > pFeature( new CSeq_feat );

    if ( ! x_FeatureSetId( gff, pFeature ) ) {
        return false;
    }
    if ( ! x_FeatureSetLocation( gff, pFeature ) ) {
        return false;
    }
    if ( ! x_FeatureSetData( gff, pFeature ) ) {
        return false;
    }
    if ( ! x_FeatureSetGffInfo( gff, pFeature ) ) {
        return false;
    }
    if ( ! x_FeatureSetQualifiers( gff, pFeature ) ) {
        return false;
    }
    
    string strId;
    if ( gff.GetAttribute( "ID", strId ) ) {
        m_MapIdToFeature[ strId ] = pFeature;
    }

    return x_AddFeatureToAnnot( pFeature, pAnnot );
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::x_FeatureSetId(
    const CGff2Record& record,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    string strId;
    if ( record.GetAttribute( "ID", strId ) ) {
        pFeature->SetId().SetLocal().SetStr( strId );
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::x_FeatureSetXref(
    const CGff2Record& record,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    string strParent;
    if ( ! record.GetAttribute( "Parent", strParent ) ) {
        return true;
    }
    CRef< CFeat_id > pFeatId( new CFeat_id );
    pFeatId->SetLocal().SetStr( strParent );
    CRef< CSeqFeatXref > pXref( new CSeqFeatXref );
    pXref->SetId( *pFeatId );
    
    pFeature->SetXref().push_back( pXref );
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::x_FeatureSetLocation(
    const CGff2Record& record,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    CRef< CSeq_id > pId;

    const string& id_str = record.Id();
    if (m_iFlags & fAllIdsAsLocal) {
        pId.Reset(new CSeq_id(CSeq_id::e_Local, id_str));
    } else {
        bool is_numeric =
            id_str.find_first_not_of("0123456789") == string::npos;

        if (is_numeric && (m_iFlags & fNumericIdsAsLocal)) {
            pId.Reset(new CSeq_id(CSeq_id::e_Local, id_str));
        }
        else {
            try {
                pId.Reset( new CSeq_id(id_str));
                if (!pId || (pId->IsGi() && pId->GetGi() < 500) ) {
                    pId = new CSeq_id(CSeq_id::e_Local, id_str);
                }
            }
            catch (CException&) {
                pId.Reset(new CSeq_id(CSeq_id::e_Local, id_str));
            }
        }
    }
    CRef< CSeq_loc > pLocation( new CSeq_loc );
    pLocation->SetInt().SetId( *pId );
    pLocation->SetInt().SetFrom( record.SeqStart() );
    pLocation->SetInt().SetTo( record.SeqStop() );
    if ( record.IsSetStrand() ) {
        pLocation->SetInt().SetStrand( record.Strand() );
    }
    pFeature->SetLocation( *pLocation );

    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::x_ProcessQualifierSpecialCase(
    CGff2Record::TAttrCit it,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    return false;
}  

//  ----------------------------------------------------------------------------
bool CGff2Reader::x_FeatureSetQualifiers(
    const CGff2Record& record,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    CRef< CGb_qual > pQual( new CGb_qual );
    pQual->SetQual( "gff_source" );
    pQual->SetVal( record.Source() );
    pFeature->SetQual().push_back( pQual );

    pQual.Reset( new CGb_qual );
    pQual->SetQual( "gff_type" );
    pQual->SetVal( record.Type() );
    pFeature->SetQual().push_back( pQual );

    if ( record.IsSetScore() ) {
        pQual.Reset( new CGb_qual );
        pQual->SetQual( "gff_score" );
        pQual->SetVal( NStr::DoubleToString( record.Score() ) );
        pFeature->SetQual().push_back( pQual );
    }

    //
    //  Create GB qualifiers for the record attributes:
    //
    const CGff2Record::TAttributes& attrs = record.Attributes();
    CGff2Record::TAttrCit it = attrs.begin();
    for ( /*NOOP*/; it != attrs.end(); ++it ) {

        // special case some well-known attributes
        if ( x_ProcessQualifierSpecialCase( it, pFeature ) ) {
            continue;
        }

        // turn everything else into a qualifier
        pQual.Reset( new CGb_qual );
        pQual->SetQual( it->first );
        pQual->SetVal( it->second );
        pFeature->SetQual().push_back( pQual );
    }    
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::x_FeatureSetGffInfo(
    const CGff2Record& record,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    CRef< CUser_object > pGffInfo( new CUser_object );
    pGffInfo->SetType().SetStr( "gff-info" );    
    pGffInfo->AddField( "gff-attributes", record.AttributesLiteral() );
    pGffInfo->AddField( "gff-start", NStr::UIntToString( record.SeqStart() ) );
    pGffInfo->AddField( "gff-stop", NStr::UIntToString( record.SeqStop() ) );
    pGffInfo->AddField( "gff-cooked", string( "false" ) );

    pFeature->SetExts().push_back( pGffInfo );
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::x_FeatureSetData(
    const CGff2Record& record,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    //
    //  Do something with the phase information --- but only for CDS features!
    //

    CSeqFeatData::ESubtype iGenbankType = SofaTypes().MapSofaTermToGenbankType(
        record.Type() );

    switch( iGenbankType ) {
    default:
        return x_FeatureSetDataMiscFeature( record, pFeature );

    case CSeqFeatData::eSubtype_cdregion:
        return x_FeatureSetDataCDS( record, pFeature );
    case CSeqFeatData::eSubtype_exon:
        return x_FeatureSetDataExon( record, pFeature );
    case CSeqFeatData::eSubtype_gene:
        return x_FeatureSetDataGene( record, pFeature );
    case CSeqFeatData::eSubtype_mRNA:
        return x_FeatureSetDataMRNA( record, pFeature );
    }    
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::x_FeatureSetDataGene(
    const CGff2Record& record,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    CGene_ref& geneRef = pFeature->SetData().SetGene();  
 
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::x_FeatureSetDataMRNA(
    const CGff2Record& record,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    CRNA_ref& rnaRef = pFeature->SetData().SetRna();
    rnaRef.SetType( CRNA_ref::eType_mRNA );

    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::x_FeatureSetDataCDS(
    const CGff2Record& record,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    CCdregion& cdr = pFeature->SetData().SetCdregion();
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::x_FeatureSetDataExon(
    const CGff2Record& record,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    CSeqFeatData& data = pFeature->SetData();
    data.SetImp().SetKey( "exon" );
    
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::x_FeatureSetDataMiscFeature(
    const CGff2Record& record,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    CSeqFeatData& data = pFeature->SetData();
    data.SetImp().SetKey( "misc_feature" );
    if ( record.IsSetPhase() ) {
        CRef< CGb_qual > pQual( new CGb_qual );
        pQual->SetQual( "gff_phase" );
        pQual->SetVal( NStr::UIntToString( record.Phase() ) );
        pFeature->SetQual().push_back( pQual );
    }  
    
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::x_GetFeatureById(
    const string & strId, 
    ncbi::CRef<CSeq_feat>& pFeature )
//  ----------------------------------------------------------------------------
{
    map< string, CRef< CSeq_feat > >::iterator it;
    it = m_MapIdToFeature.find(strId);
	if(it != m_MapIdToFeature.end()) {
        pFeature = it->second;
		return true;
	}
    return false;
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::x_HasTemporaryLocation(
    const CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    if ( ! feature.CanGetExts() ) {
        return false;
    }
    list< CRef< CUser_object > > pExts = feature.GetExts();
    list< CRef< CUser_object > >::iterator it;
    for ( it = pExts.begin(); it != pExts.end(); ++it ) {
        if ( ! (*it)->CanGetType() || ! (*it)->GetType().IsStr() ) {
            continue;
        }
        if ( (*it)->GetType().GetStr() != "gff-info" ) {
            continue;
        }
        if ( ! (*it)->HasField( "gff-cooked" ) ) {
            return false;
        }
        return ( (*it)->GetField( "gff-cooked" ).GetData().GetStr() == "false" );
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::IsExon(
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    if ( ! pFeature->CanGetData() || ! pFeature->GetData().IsImp() ) {
        return false;
    }
    return ( pFeature->GetData().GetImp().GetKey() == "exon" );
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::x_AddFeatureToAnnot(
    CRef< CSeq_feat > pFeature,
    CRef< CSeq_annot > pAnnot )
//  ----------------------------------------------------------------------------
{
    if (IsExon(pFeature)) {
        CRef< CSeq_feat > pParent;    
        if ( ! x_GetParentFeature( *pFeature, pParent ) ) {
            pAnnot->SetData().SetFtable().push_back( pFeature ) ;
            return true;
        }
        return x_FeatureMergeExon( pFeature, pParent );
    }

    pAnnot->SetData().SetFtable().push_back( pFeature ) ;
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::x_GetParentFeature(
    const CSeq_feat& feature,
    CRef< CSeq_feat >& pParent )
//  ----------------------------------------------------------------------------
{
    if ( ! feature.CanGetQual() ) {
        return false;
    }

    string strParentId;
    vector< CRef< CGb_qual > > quals = feature.GetQual();
    vector< CRef< CGb_qual > >::iterator it;
    for ( it = quals.begin(); it != quals.end(); ++it ) {
        if ( (*it)->CanGetQual() && (*it)->GetQual() == "Parent" ) {
            strParentId = (*it)->GetVal();
            break;
        }
    }
    if ( it == quals.end() ) {
        return false;
    }
    if ( ! x_GetFeatureById( strParentId, pParent ) ) {
        return false;
    }
    return true;
}

//  ---------------------------------------------------------------------------
bool CGff2Reader::x_FeatureMergeExon(
    CRef< CSeq_feat > pExon,
    CRef< CSeq_feat > pFeature )
//  ---------------------------------------------------------------------------
{
    if ( x_HasTemporaryLocation( *pFeature ) ) {
        // start rebuilding parent location from scratch
        pFeature->SetLocation().Assign( pExon->GetLocation() );
        list< CRef< CUser_object > > pExts = pFeature->SetExts();
        list< CRef< CUser_object > >::iterator it;
        for ( it = pExts.begin(); it != pExts.end(); ++it ) {
            if ( ! (*it)->CanGetType() || ! (*it)->GetType().IsStr() ) {
                continue;
            }
            if ( (*it)->GetType().GetStr() != "gff-info" ) {
                continue;
            }
            (*it)->SetField( "gff-cooked" ).SetData().SetStr( "true" );
        }
    }
    else {
        // add exon location to current parent location
        pFeature->SetLocation().Add(  pExon->GetLocation() );
    }

    return true;
}
                                
//  ============================================================================
CRef< CDbtag >
CGff2Reader::x_ParseDbtag(
    const string& str )
//  ============================================================================
{
    CRef< CDbtag > pDbtag( new CDbtag() );
    string strDb, strTag;
    NStr::SplitInTwo( str, ":", strDb, strTag );

    // dbtag names for Gff2 do not always match the names for genbank.
    // special case known fixups here:
    if ( strDb == "NCBI_gi" ) {
        strDb = "GI";
    }
    // todo: all the other ones


    if ( ! strTag.empty() ) {
        pDbtag->SetDb( strDb );
        try {
            pDbtag->SetTag().SetId( NStr::StringToUInt( strTag ) );
        }
        catch ( ... ) {
            pDbtag->SetTag().SetStr( strTag );
        }
    }
    else {
        pDbtag->SetDb( "unknown" );
        pDbtag->SetTag().SetStr( str );
    }
    return pDbtag;
}

END_objects_SCOPE
END_NCBI_SCOPE
