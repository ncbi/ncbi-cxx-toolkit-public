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

#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Score.hpp>

#include <objtools/readers/read_util.hpp>
#include <objtools/readers/reader_exception.hpp>
#include <objtools/readers/line_error.hpp>
#include <objtools/readers/message_listener.hpp>
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
    IMessageListener* pMessageListener )
//  ---------------------------------------------------------------------------
{
    CStreamLineReader lr( istr );
    ReadSeqAnnots( annots, lr, pMessageListener );
}

//  ---------------------------------------------------------------------------                       
void
CGff2Reader::ReadSeqAnnots(
    vector< CRef<CSeq_annot> >& annots,
    ILineReader& lr,
    IMessageListener* pMessageListener )
//  ----------------------------------------------------------------------------
{
    if ( m_iFlags & fNewCode ) {
        return ReadSeqAnnotsNew( annots, lr, pMessageListener );
    }
    CRef< CSeq_entry > entry = ReadSeqEntry( lr, pMessageListener );
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
    IMessageListener* pEC )
//  ----------------------------------------------------------------------------
{
    string line;
    //int linecount = 0;

    xReadInit();
    while ( ! lr.AtEOF() ) {
        ++m_uLineNumber;
        line = NStr::TruncateSpaces_Unsafe( *++lr );
        if ( line.empty() ) {
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
            if ( ! x_ParseDataGff(line, annots, pEC) ) {
                continue;
            }
        }
        catch( CObjReaderLineException& err ) {
            err.SetLineNumber( m_uLineNumber );
            ProcessError(err, pEC);
        }
    }
    typedef vector< CRef<CSeq_annot> >::iterator ANNOTIT;
    for (ANNOTIT it = annots.begin(); it != annots.end(); ++it) {
        try {
            xAnnotPostProcess(*it);
        }
        catch(CObjReaderLineException& err) {
            err.SetLineNumber(m_uLineNumber);
            ProcessError(err, pEC);
        }
    }
}

//  ----------------------------------------------------------------------------                
CRef< CSeq_entry >
CGff2Reader::ReadSeqEntry(
    ILineReader& lr,
    IMessageListener* pMessageListener ) 
//  ----------------------------------------------------------------------------                
{ 
    vector<CRef<CSeq_annot> > annots;
    ReadSeqAnnotsNew( annots, lr, pMessageListener );
    
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
    IMessageListener* pMessageListener ) 
//  ----------------------------------------------------------------------------                
{ 
    CRef<CSerialObject> object( 
        ReadSeqEntry( lr, pMessageListener ).ReleaseOrNull() );
    return object;
}
    
//  ----------------------------------------------------------------------------
bool CGff2Reader::x_ReadLine(
    ILineReader& lr,
    string& strLine )
//  ----------------------------------------------------------------------------
{
    strLine.clear();
    while ( ! lr.AtEOF() ) {
        CTempString temp = NStr::TruncateSpaces_Unsafe( *++lr );
        ++m_uLineNumber;
        if ( ! x_IsCommentLine( temp ) ) {
            strLine = temp;
            return true;
        }
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::x_IsCommentLine(
    const CTempString& strLine )
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
bool CGff2Reader::x_ParseDataGff(
    const string& strLine,
    TAnnots& annots,
    IMessageListener* pEC)
//  ----------------------------------------------------------------------------
{
    if ( CGff2Reader::IsAlignmentData(strLine) ) {
        return x_ParseAlignmentGff(strLine, annots);
    }
    return x_ParseFeatureGff(strLine, annots, pEC);
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::x_ParseFeatureGff(
    const string& strLine,
    TAnnots& annots,
    IMessageListener* pEC)
//  ----------------------------------------------------------------------------
{
    //
    //  Parse the record and determine which ID the given feature will pertain 
    //  to:
    //
    CGff2Record* pRecord = x_CreateRecord();
    try {
        pRecord->AssignFromGff(strLine);
    }
    catch(CObjReaderLineException& err) {
        ProcessError(err, pEC);
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
        if ( ! x_UpdateAnnotFeature( *pRecord, *it ) ) {
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
bool CGff2Reader::x_ParseAlignmentGff(
    const string& strLine,
    TAnnots& annots )
//  ----------------------------------------------------------------------------
{
    //
    //  Parse the record and determine which ID the given feature will pertain 
    //  to:
    //
    auto_ptr<CGff2Record> pRecord(x_CreateRecord());
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
        if ( ! x_UpdateAnnotAlignment( *pRecord, *it ) ) {
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
bool CGff2Reader::x_InitAnnot(
    const CGff2Record& gff,
    CRef< CSeq_annot > pAnnot )
//  ----------------------------------------------------------------------------
{
    CRef< CAnnot_id > pAnnotId( new CAnnot_id );
    pAnnotId->SetLocal().SetStr( gff.Id() );
    pAnnot->SetId().push_back( pAnnotId );
    //pAnnot->SetData().SetFtable();

    // if available, add current browser information
    if ( m_CurrentBrowserInfo ) {
        pAnnot->SetDesc().Set().push_back( m_CurrentBrowserInfo );
    }

    // if available, add current track information
    if ( m_CurrentTrackInfo ) {
        pAnnot->SetDesc().Set().push_back( m_CurrentTrackInfo );
    }

    if ( !m_AnnotName.empty() ) {
        pAnnot->SetNameDesc(m_AnnotName);
    }
    if ( !m_AnnotTitle.empty() ) {
        pAnnot->SetTitleDesc(m_AnnotTitle);
    }

    if (gff.IsAlignmentRecord()) {
        pAnnot->SetData().SetAlign();
        return x_UpdateAnnotAlignment( gff, pAnnot );
    }
    else {
        pAnnot->SetData().SetFtable();
        return x_UpdateAnnotFeature( gff, pAnnot );
    }
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::x_UpdateAnnotFeature(
    const CGff2Record& gff,
    CRef< CSeq_annot > pAnnot,
    IMessageListener* pEC)
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
    if (!x_FeatureSetXref(gff, pFeature)) {
        return false;
    }
    if (!x_AddFeatureToAnnot( pFeature, pAnnot )) {
        return false;
    }
    string strId;
    if (gff.GetAttribute("ID", strId) ) {
        if (m_MapIdToFeature.find(strId) == m_MapIdToFeature.end()) {
            m_MapIdToFeature[strId] = pFeature;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::x_UpdateAnnotAlignment(
    const CGff2Record& gff,
    CRef< CSeq_annot > pAnnot )
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_align> pAlign( new CSeq_align );
    pAlign->SetType(CSeq_align::eType_partial);
    pAlign->SetDim(2);

    //score
    if (!xAlignmentSetScore(gff, pAlign)) {
        return false;
    }
    if (!xAlignmentSetSegment(gff, pAlign)) {
        return false;
    }
    pAnnot->SetData().SetAlign().push_back( pAlign ) ;
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::xAlignmentSetSegment(
    const CGff2Record& gff,
    CRef<CSeq_align> pAlign)
//  ----------------------------------------------------------------------------
{
    string targetInfo;
    vector<string> targetParts;
    if (!gff.GetAttribute("Target", targetInfo)) {
        return false;
    }
    NStr::Tokenize(targetInfo, " ", targetParts);
    if (targetParts.size() != 4) {
        return false;
    }
    
    string gapInfo;
    vector<string> gapParts;
    if (!gff.GetAttribute("Gap", gapInfo)) {
        return false;
    }
    NStr::Tokenize(gapInfo, " ", gapParts);
    int gapCount = gapParts.size();

    //meta
    CSeq_align::TSegs& segs = pAlign->SetSegs();
    CSeq_align::C_Segs::TDenseg& denseg = segs.SetDenseg();
    denseg.SetDim(2);
    denseg.SetNumseg(gapCount);

    //ids
    denseg.SetIds().push_back(
        CRef<CSeq_id>(new CSeq_id(targetParts[0])));
    denseg.SetIds().push_back(
        CRef<CSeq_id>(new CSeq_id(gff.Id())));

    //starts
    size_t targetOffset = 0;
    size_t identOffset = gff.SeqStart();
    for (int i=0; i < gapCount; ++i) {
        char changeType = gapParts[i][0];
        int changeSize = NStr::StringToInt(gapParts[i].substr(1));
        switch (changeType) {
        default:
            return false;
        case 'M':
            denseg.SetStarts().push_back(targetOffset);
            denseg.SetStarts().push_back(identOffset);
            targetOffset += changeSize;
            identOffset += changeSize;
            break;
        case 'I':
            denseg.SetStarts().push_back(targetOffset);
            denseg.SetStarts().push_back(-1);
            targetOffset += changeSize;
            break;
        case 'D':
            denseg.SetStarts().push_back(-1);
            denseg.SetStarts().push_back(identOffset);
            identOffset += changeSize;
            break;
        }
    }
    //lengths
    for (int i=0; i < gapCount; ++i) {
        denseg.SetLens().push_back(NStr::StringToInt(gapParts[i].substr(1)));
    }
    //strands
    ENa_strand targetStrand = eNa_strand_plus;
    if (targetParts[3] == "-") {
        targetStrand = eNa_strand_minus;
    }
    ENa_strand identStrand = eNa_strand_plus;
    if (gff.IsSetStrand()) {
        identStrand = gff.Strand();
    }
    for (int i=0; i < gapCount; ++i) {
        denseg.SetStrands().push_back(targetStrand);
        denseg.SetStrands().push_back(identStrand);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::xAlignmentSetScore(
    const CGff2Record& gff,
    CRef<CSeq_align> pAlign)
//  ----------------------------------------------------------------------------
{
    if (gff.IsSetScore()) {
        pAlign->SetNamedScore(CSeq_align::eScore_Score, 
            int(gff.Score()));
    }

    string extraScore;

    const string intScores[] = {
        //official
        "score",
        "align_length",
        "num_ident",
        "num_positives",
        "num_negatives",
        "num_mismatch",
        "num_gap",

        //picked up from real data files
        "common_component",
        "filter_score",
        "for_remapping",
        "merge_aligner",
        "rank",
        "reciprocity",
    };

    const size_t intCount(sizeof(intScores)/sizeof(string));
    for (size_t i=0; i < intCount; ++i) {
        if (gff.GetAttribute(intScores[i], extraScore)) {
            pAlign->SetNamedScore(
                intScores[i], int(NStr::StringToDouble(extraScore)));
        }
    }

    const string realScores[] = {
        //official
        "bit_score",
        "e_value",
        "pct_identity_gap",
        "pct_identity_ungap",
        "pct_identity_gapopen_only",
        "pct_coverage",
        "sum_e",
        "comp_adjustment_method",
        "pct_coverage_hiqual",

        //picked up from real data files
    };

    const size_t realCount(sizeof(realScores)/sizeof(string));
    for (size_t i=0; i < realCount; ++i) {
        if (gff.GetAttribute(realScores[i], extraScore)) {
            pAlign->SetNamedScore(
                realScores[i], NStr::StringToDouble(extraScore));
        }
    }

    return true;
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
    list<string> parents;
    if (!record.GetAttribute("Parent", parents)) {
        return true;
    }
    for (list<string>::const_iterator cit = parents.begin(); cit != parents.end();
            ++cit) {
        CRef<CFeat_id> pFeatId(new CFeat_id);
        pFeatId->SetLocal().SetStr(*cit);
        IdToFeatureMap::iterator it = m_MapIdToFeature.find(*cit);
        if (it == m_MapIdToFeature.end()) {
            return false;
        }
        CRef<CSeq_feat> pParent = it->second;
        pParent->SetId(*pFeatId);
        CRef< CSeqFeatXref > pXref(new CSeqFeatXref);
        pXref->SetId(*pFeatId);  
        pFeature->SetXref().push_back(pXref);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::x_FeatureSetLocation(
    const CGff2Record& record,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    CRef< CSeq_id > pId = CReadUtil::AsSeqId(record.Id(), m_iFlags);
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
bool CGff2Reader::x_FeatureTrimQualifiers(
    const CGff2Record& record,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    typedef CSeq_feat::TQual TQual;
    typedef CGff2Record::TAttributes TAttrs;
    //task:
    // for each attribute of the new piece check if we already got a feature 
    //  qualifier
    // if so, and with the same value, then the qualifier is allowed to live
    // otherwise it is subfeature specific and hence removed from the feature
    TQual& quals = pFeature->SetQual();
    const TAttrs& attrs = record.Attributes();
    for (TQual::iterator it = quals.begin(); it != quals.end(); /**/) {
        const string& qualKey = (*it)->GetQual();
        if (NStr::StartsWith(qualKey, "gff_")) {
            it++;
            continue;
        }
        const string& qualVal = (*it)->GetVal();
        string attrVal;
        if (!record.GetAttribute(qualKey, attrVal)) {
            //superfluous qualifier- squish
            it = quals.erase(it);
            continue;
        }
        if (qualVal != attrVal) {
            //ambiguous qualifier- squish
            it = quals.erase(it);
            continue;
        }
        it++;
    }
    return true;
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
    pGffInfo->AddField( "gff-start", NStr::NumericToString( record.SeqStart() ) );
    pGffInfo->AddField( "gff-stop", NStr::NumericToString( record.SeqStop() ) );
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
    pFeature->SetData().SetGene();
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
    pFeature->SetData().SetCdregion();
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
    CRef<CSeq_feat> pFeature )
//  ----------------------------------------------------------------------------
{
    if (!pFeature->CanGetData() || !pFeature->GetData().IsImp()) {
        return false;
    }
    return (pFeature->GetData().GetImp().GetKey() == "exon" );
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::IsCds(
    CRef<CSeq_feat> pFeature)
//  ----------------------------------------------------------------------------
{
    if (!pFeature->CanGetData()) {
        return false;
    }
    return (pFeature->GetData().GetSubtype() == CSeqFeatData::eSubtype_cdregion);
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::x_AddFeatureToAnnot(
    CRef< CSeq_feat > pFeature,
    CRef< CSeq_annot > pAnnot )
//  ----------------------------------------------------------------------------
{
    if (IsExon(pFeature)) {
        CRef< CSeq_feat > pParent;    
        if (!x_GetParentFeature(*pFeature, pParent) ) {
            pAnnot->SetData().SetFtable().push_back(pFeature) ;
            return true;
        }
        return xFeatureMergeExon( pFeature, pParent );
    }
    if (IsCds(pFeature)) {
        CRef<CSeq_feat> pExisting;
        if (!xGetExistingFeature(*pFeature, pAnnot, pExisting)) {
            pAnnot->SetData().SetFtable().push_back(pFeature) ;
            return true;
        }
        return xFeatureMergeCds(pFeature, pExisting);
    }
    pAnnot->SetData().SetFtable().push_back( pFeature ) ;
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::xGetExistingFeature(
    const CSeq_feat& feature,
    CRef<CSeq_annot> pAnnot,
    CRef< CSeq_feat >& pExisting )
//  ----------------------------------------------------------------------------
{
    if (!feature.CanGetQual()) {
        return false;
    }

    string strExistingId = feature.GetNamedQual("ID");
    if (strExistingId.empty()) {
        return false;
    }
    if (!x_GetFeatureById( strExistingId, pExisting)) {
        return false;
    }
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

    string strParentId = feature.GetNamedQual("Parent");
    if (strParentId.empty()) {
        return false;
    }
    if ( ! x_GetFeatureById( strParentId, pParent ) ) {
        return false;
    }
    return true;
}

//  ---------------------------------------------------------------------------
bool CGff2Reader::xFeatureMergeExon(
    CRef< CSeq_feat > pExon,
    CRef< CSeq_feat > pMrna )
//  ---------------------------------------------------------------------------
{
    if ( x_HasTemporaryLocation( *pMrna ) ) {
        // start rebuilding parent location from scratch
        pMrna->SetLocation().Assign( pExon->GetLocation() );
        list< CRef< CUser_object > > pExts = pMrna->SetExts();
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
        pMrna->SetLocation().Add(  pExon->GetLocation() );
    }

    return true;
}
                                
//  ---------------------------------------------------------------------------
bool CGff2Reader::xFeatureMergeCds(
    CRef< CSeq_feat > pNewPiece,
    CRef< CSeq_feat > pExisting )
//  ---------------------------------------------------------------------------
{
    pExisting->SetLocation().Add(pNewPiece->GetLocation());
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

//  ============================================================================
bool CGff2Reader::xAnnotPostProcess(
    CRef<CSeq_annot> pAnnot)
//  ============================================================================
{
    return true;
}

//  ============================================================================
bool CGff2Reader::xReadInit()
//  ============================================================================
{
    return true;
}

//  ============================================================================
bool CGff2Reader::IsAlignmentData(
    const string& line)
//  ============================================================================
{
    vector<CTempString> columns;
    CGff2Record::TokenizeGFF(columns, line);
    if (columns.size() < 9) {
        return false;
    }
    if (NStr::StartsWith(columns[2], "match")) {
        return true;
    }
    return false;
}

END_objects_SCOPE
END_NCBI_SCOPE
