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
#include <objtools/readers/gtf_reader.hpp>
#include <objtools/error_codes.hpp>

#include <algorithm>

#define NCBI_USE_ERRCODE_X   Objtools_Rd_RepMask

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
string s_GeneKey(
    const CGff3Record& gff )
//  ----------------------------------------------------------------------------
{
    string strGeneId;
    if ( ! gff.GetAttribute( "gene_id", strGeneId ) ) {
        cerr << "Unexpected: GTF CDS feature without a gene_id." << endl;
        return false;
    }
    return strGeneId;
}

//  ----------------------------------------------------------------------------
string s_CdsKey(
    const CGff3Record& gff )
//  ----------------------------------------------------------------------------
{
    string strGeneId;
    if ( ! gff.GetAttribute( "gene_id", strGeneId ) ) {
        cerr << "Unexpected: GTF CDS feature without a gene_id." << endl;
        return false;
    }
    string strTranscriptId;
    if ( ! gff.GetAttribute( "transcript_id", strTranscriptId ) ) {
        cerr << "Unexpected: GTF CDS feature without a transcript_id." << endl;
        return false;
    }
    return strGeneId + "|" + strTranscriptId;
}

//  ----------------------------------------------------------------------------
CGtfReader::CGtfReader():
//  ----------------------------------------------------------------------------
    CGff3Reader( fNewCode )
{
}

//  ----------------------------------------------------------------------------
CGtfReader::~CGtfReader()
//  ----------------------------------------------------------------------------
{
}

//  ---------------------------------------------------------------------------                       
void
CGtfReader::ReadSeqAnnots(
    vector< CRef<CSeq_annot> >& annots,
    ILineReader& lr,
    IErrorContainer* pErrorContainer )
//  ----------------------------------------------------------------------------
{
    string line;
    int linecount = 0;

    while ( x_GetLine( lr, line, linecount ) ) {
        try {
            if ( x_ParseBrowserLineGff( line, m_CurrentBrowserInfo ) ) {
                continue;
            }
            if ( x_ParseTrackLineGff( line, m_CurrentTrackInfo ) ) {
                continue;
            }
            x_ParseFeatureGff( line, annots );
        }
        catch( CObjReaderLineException& err ) {
            err.SetLineNumber( linecount );
        }
        continue;
    }
    x_AddConversionInfoGff( annots, &m_ErrorsPrivate );
}

//  --------------------------------------------------------------------------- 
void
CGtfReader::ReadSeqAnnots(
    vector< CRef<CSeq_annot> >& annots,
    CNcbiIstream& istr,
    IErrorContainer* pErrorContainer )
//  ---------------------------------------------------------------------------
{
    CStreamLineReader lr( istr );
    ReadSeqAnnots( annots, lr, pErrorContainer );
}

//  ---------------------------------------------------------------------------                       
bool
CGtfReader::x_GetLine(
    ncbi::ILineReader& lr,
    string& strLine,
    int& iLineCount )
//  ---------------------------------------------------------------------------
{
    while ( ! lr.AtEOF() ) {

        string strBuffer = NStr::TruncateSpaces( *++lr );
        ++iLineCount;

        if ( NStr::TruncateSpaces( strBuffer ).empty() ) {
            continue;
        }
        size_t uComment = strBuffer.find( '#' );
        if ( uComment != NPOS ) {
            strBuffer = strBuffer.substr( 0, uComment );
            if ( strBuffer.empty() ) {
                continue;
            }
        }  

        strLine = strBuffer;
        return true;    
    }
    return false;
}
 
//  ----------------------------------------------------------------------------
bool CGtfReader::x_UpdateAnnot(
    const CGff3Record& gff,
    CRef< CSeq_annot > pAnnot )
//  ----------------------------------------------------------------------------
{
    CRef< CSeq_feat > pFeature( new CSeq_feat );

    //
    // Handle officially recognized GTF types:
    //
    string strType = gff.Type();
    if ( strType == "CDS" ) {
        //
        // Observations:
        // Location does not include the stop codon hence must be fixed up once
        //  the stop codon is seen.
        //
        return x_UpdateAnnotCds( gff, pAnnot );
    }
    if ( strType == "start_codon" ) {
        //
        // Observation:
        // Comes in up to three pieces (depending on splicing).
        // Location _is_ included in CDS.
        //
        return x_UpdateAnnotStartCodon( gff, pAnnot );
    }
    if ( strType == "stop_codon" ) {
        //
        // Observation:
        // Comes in up to three pieces (depending on splicing).
        // Location not included in CDS hence must be used to fix up location of
        //  the coding region.
        //
        return x_UpdateAnnotStopCodon( gff, pAnnot );
    }
    if ( strType == "5UTR" ) {
        return x_UpdateAnnot5utr( gff, pAnnot );
    }
    if ( strType == "3UTR" ) {
        return x_UpdateAnnot3utr( gff, pAnnot );
    }
    if ( strType == "inter" ) {
        return x_UpdateAnnotInter( gff, pAnnot );
    }
    if ( strType == "inter_CNS" ) {
        return x_UpdateAnnotInterCns( gff, pAnnot );
    }
    if ( strType == "intron_CNS" ) {
        return x_UpdateAnnotIntronCns( gff, pAnnot );
    }
    if ( strType == "exon" ) {
        return x_UpdateAnnotExon( gff, pAnnot );
    }

    //
    //  Every other type is not officially sanctioned GTF, and per spec we are
    //  supposed to ignore it. In the spirit of being lenient on input we may
    //  try to salvage some of it anyway.
    //
    if ( strType == "gene" ) {
        //
        // Not an official GTF feature type but seen frequently. Hence we give
        //  it some recognition.
        //
        if ( ! x_CreateParentGene( gff, pAnnot ) ) {
            return false;
        }
    }
    return x_UpdateAnnotMiscFeature( gff, pAnnot );
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_UpdateAnnotCds(
    const CGff3Record& gff,
    CRef< CSeq_annot > pAnnot )
//  ----------------------------------------------------------------------------
{

    //
    // If there is no gene feature to go with this CDS then make one. Otherwise,
    //  make sure the existing gene feature includes the location of the CDS.
    //
    CRef< CSeq_feat > pGene;
    if ( ! x_FindParentGene( gff, pGene ) ) {
        if ( ! x_CreateParentGene( gff, pAnnot ) ) {
            return false;
        }
    }
    else {
        if ( ! x_MergeParentGene( gff, pGene ) ) {
            return false;
        }
    }
    
    //
    // If there is not CDS feature with this gene_id|transcript_id then make one.
    //  Otherwise, fix up the location of the existing one.
    //
    string strTranscriptId;
    if ( ! gff.GetAttribute( "transcript_id", strTranscriptId ) ) {
        cerr << "Unexpected: GTF CDS feature without a transcript_id." << endl;
        return false;
    }
    string strCdsId = s_CdsKey( gff );
    TIdToFeature::iterator cds_it = m_CdsMap.find( strCdsId );
    if ( cds_it == m_CdsMap.end() ) {
        //
        // Create a brand new CDS feature:
        //
        if ( ! x_CreateParentCds( gff, pAnnot ) ) {
            return false;
        }
    }
    else {
        //
        // Update an already existing CDS features:
        //
        if ( ! x_MergeFeatureLocationMultiInterval( gff, cds_it->second ) ) {
            return false;
        }
    }

    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_UpdateAnnotStartCodon(
    const CGff3Record& gff,
    CRef< CSeq_annot > pAnnot )
//  ----------------------------------------------------------------------------
{
    //
    // From the spec, the start codon should have already been accounted for in
    //  one of the coding regions. Hence, there is nothing left to do.
    //
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_UpdateAnnotStopCodon(
    const CGff3Record& gff,
    CRef< CSeq_annot > pAnnot )
//  ----------------------------------------------------------------------------
{
    //
    // From the spec, the stop codon has _not_ been accounted for in any of the 
    //  coding regions. Hence, we will treat (pieces of) the stop codon just
    //  like (pieces of) CDS.
    //
    return x_UpdateAnnotCds( gff, pAnnot );
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_UpdateAnnot5utr(
    const CGff3Record& gff,
    CRef< CSeq_annot > pAnnot )
//  ----------------------------------------------------------------------------
{
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_UpdateAnnot3utr(
    const CGff3Record& gff,
    CRef< CSeq_annot > pAnnot )
//  ----------------------------------------------------------------------------
{
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_UpdateAnnotInter(
    const CGff3Record& gff,
    CRef< CSeq_annot > pAnnot )
//  ----------------------------------------------------------------------------
{
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_UpdateAnnotInterCns(
    const CGff3Record& gff,
    CRef< CSeq_annot > pAnnot )
//  ----------------------------------------------------------------------------
{
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_UpdateAnnotIntronCns(
    const CGff3Record& gff,
    CRef< CSeq_annot > pAnnot )
//  ----------------------------------------------------------------------------
{
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_UpdateAnnotExon(
    const CGff3Record& gff,
    CRef< CSeq_annot > pAnnot )
//  ----------------------------------------------------------------------------
{
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_UpdateAnnotMiscFeature(
    const CGff3Record& gff,
    CRef< CSeq_annot > pAnnot )
//  ----------------------------------------------------------------------------
{
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_UpdateFeatureId(
    const CGff3Record& record,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    string strGeneId;
    if ( ! record.GetAttribute( "gene_id", strGeneId ) ) {
        cerr << "Unexpected: Feature without gene_id" << endl;
        return false;
    }
    if ( record.Type() == "gene" ) {
        pFeature->SetId().SetLocal().SetStr( string( "gene." ) + strGeneId );
        return true;
    }

    string strTranscriptId;
    if ( ! record.GetAttribute( "transcript_id", strTranscriptId ) ) {
        cerr << "Unexpected: Feature without transcript_id" << endl;
        return false;
    }

    if ( record.Type() == "CDS" ) {
        pFeature->SetId().SetLocal().SetStr( 
            string( "cds." ) + strGeneId + "." + strTranscriptId );
        return true;
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_CreateFeatureLocation(
    const CGff3Record& record,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    CRef< CSeq_id > pId;

    const string& id_str = record.Id();
    if (m_uFlags & fAllIdsAsLocal) {
        pId.Reset(new CSeq_id(CSeq_id::e_Local, id_str));
    } else {
        bool is_numeric =
            id_str.find_first_not_of("0123456789") == string::npos;

        if (is_numeric) {
            pId.Reset(new CSeq_id(CSeq_id::e_Local, id_str));
        }
        else {
            try {
                pId.Reset( new CSeq_id(id_str));
            }
            catch (CException&) {
                pId.Reset(new CSeq_id(CSeq_id::e_Local, id_str));
            }
        }
    }
    CSeq_interval& location = pFeature->SetLocation().SetInt();
    location.SetId( *pId );
    location.SetFrom( record.SeqStart() );
    location.SetTo( record.SeqStop() );
    if ( record.IsSetStrand() ) {
        location.SetStrand( record.Strand() );
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_CreateGeneXref(
    const CGff3Record& record,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    string strGeneId;
    if ( ! record.GetAttribute( "gene_id", strGeneId ) ) {
        return true;
    }
    CRef< CSeq_feat > pParent;
    if ( ! x_FindParentGene( record, pParent ) ) {
        return true;
    }
    
    CRef< CSeqFeatXref > pXref( new CSeqFeatXref );
    pXref->SetId( pParent->SetId() );    
    pFeature->SetXref().push_back( pXref );
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_MergeFeatureLocationSingleInterval(
    const CGff3Record& record,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    const CSeq_interval& gene_int = pFeature->GetLocation().GetInt();
    if ( gene_int.GetFrom() > record.SeqStart() -1 ) {
        pFeature->SetLocation().SetInt().SetFrom( record.SeqStart() - 1 );
    }
    if ( gene_int.GetTo() < record.SeqStop() - 1 ) {
        pFeature->SetLocation().SetInt().SetTo( record.SeqStop() - 1 );
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_MergeFeatureLocationMultiInterval(
    const CGff3Record& record,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    CRef< CSeq_id > pId( new CSeq_id( CSeq_id::e_Local, record.Id() ) );
    CRef< CSeq_loc > pLocation( new CSeq_loc );
    pLocation->SetInt().SetId( *pId );
    pLocation->SetInt().SetFrom( record.SeqStart() );
    pLocation->SetInt().SetTo( record.SeqStop() );
    if ( record.IsSetStrand() ) {
        pLocation->SetInt().SetStrand( record.Strand() );
    }
    pLocation = pLocation->Add( 
        pFeature->SetLocation(), CSeq_loc::fSortAndMerge_All, 0 );
    pFeature->SetLocation( *pLocation );
    return true;
}

//  -----------------------------------------------------------------------------
bool CGtfReader::x_CreateParentGene(
    const CGff3Record& gff,
    CRef< CSeq_annot > pAnnot )
//  -----------------------------------------------------------------------------
{
    //
    // Create a single gene feature:
    //
    CRef< CSeq_feat > pFeature( new CSeq_feat );

    if ( ! x_FeatureSetDataGene( gff, pFeature ) ) {
        return false;
    }
    if ( ! x_CreateFeatureLocation( gff, pFeature ) ) {
        return false;
    }
    if ( ! x_UpdateFeatureId( gff, pFeature ) ) {
        return false;
    }
    if ( ! x_FeatureSetQualifiers( gff, pFeature ) ) {
        return false;
    }

    string strGeneId;
    if ( gff.GetAttribute( "gene_id", strGeneId ) ) {
        m_GeneMap[ strGeneId ] = pFeature;
    }

    return x_AddFeatureToAnnot( pFeature, pAnnot );
}
    
//  ----------------------------------------------------------------------------
bool CGtfReader::x_MergeParentGene(
    const CGff3Record& record,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    return x_MergeFeatureLocationSingleInterval( record, pFeature );
}

//  -----------------------------------------------------------------------------
bool CGtfReader::x_CreateParentCds(
    const CGff3Record& gff,
    CRef< CSeq_annot > pAnnot )
//  -----------------------------------------------------------------------------
{
    //
    // Create a single cds feature:
    //
    CRef< CSeq_feat > pFeature( new CSeq_feat );

    if ( ! x_FeatureSetDataCDS( gff, pFeature ) ) {
        return false;
    }
    if ( ! x_CreateFeatureLocation( gff, pFeature ) ) {
        return false;
    }
    if ( ! x_UpdateFeatureId( gff, pFeature ) ) {
        return false;
    }
    if ( ! x_CreateGeneXref( gff, pFeature ) ) {
        return false;
    }
    if ( ! x_FeatureSetQualifiers( gff, pFeature ) ) {
        return false;
    }

    m_CdsMap[ s_CdsKey( gff ) ] = pFeature;

    return x_AddFeatureToAnnot( pFeature, pAnnot );
}

//  ----------------------------------------------------------------------------
bool CGtfReader::x_FindParentGene(
    const CGff3Record& gff,
    CRef< CSeq_feat >& pFeature )
//  ----------------------------------------------------------------------------
{
    string strGeneId;
    if ( ! gff.GetAttribute( "gene_id", strGeneId ) ) {
        cerr << "Unexpected: GTF CDS feature without a gene_id." << endl;
        return false;
    }
    TIdToFeature::iterator gene_it = m_GeneMap.find( strGeneId );
    if ( gene_it == m_GeneMap.end() ) {
        return false;
    }
    pFeature = gene_it->second;
    return true;
}

END_objects_SCOPE
END_NCBI_SCOPE
