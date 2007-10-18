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
 *   Repeat Masker file reader
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbiexpt.hpp>

#include <util/static_map.hpp>

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
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/Gene_ref.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Trna_ext.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Gb_qual.hpp>

#include <objtools/readers/reader_exception.hpp>
#include <objtools/readers/rm_reader.hpp>
#include <objtools/error_codes.hpp>

#include <algorithm>


#define NCBI_USE_ERRCODE_X   Objtools_Rd_RepMask

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

struct CMaskData;

//-----------------------------------------------------------------------------
class CRmOutReader: public CRmReader
//-----------------------------------------------------------------------------
{
    friend CRmReader* CRmReader::OpenReader( CNcbiIstream& );
    
    //
    //  object management:
    //
protected:
    CRmOutReader( CNcbiIstream& );
public:
    virtual ~CRmOutReader();
    
    //
    //  interface:
    //
public:
    virtual void Read( CRef<CSeq_annot>, TFlags flags = fDefaults);

    //
    //  internal helpers:
    //
protected:
    virtual bool IsHeaderLine( const string& );
    virtual bool IsIgnoredLine( const string& ); 
    
    virtual bool ParseRecord( const string& record, CMaskData& );
    virtual bool VerifyData( const CMaskData& );
    virtual bool MakeFeature( const CMaskData&, CRef<CSeq_feat>&, TFlags flags);
    
    //
    //  data:
    //
protected:
    static const unsigned long BUFFERSIZE = 256;
    char pReadBuffer[ BUFFERSIZE ];
};

//-----------------------------------------------------------------------------
struct CMaskData
//-----------------------------------------------------------------------------
{
    unsigned long sw_score;
    unsigned long outer_pos_begin;
    unsigned long outer_pos_end;
    double perc_div;
    double perc_del;
    double perc_ins;
    string query_sequence;
    string strand;
    string matching_repeat;
    string repeat_class_family;
};
    

CRmReader::CRmReader( CNcbiIstream& InStream )
    :
    m_InStream( InStream )
{
}


CRmReader::~CRmReader()
{
}


CRmOutReader::CRmOutReader( CNcbiIstream& InStream )
    :
    CRmReader( InStream )
{
}


CRmOutReader::~CRmOutReader()
{
}


void CRmOutReader::Read( CRef<CSeq_annot> entry, TFlags flags )
{
    const size_t MAX_ERROR_COUNT = 5;
    
    string line;
    CSeq_annot::C_Data::TFtable& ftable = entry->SetData().SetFtable();
    CRef<CSeq_feat> feat;
    
    size_t line_counter = 0;
    size_t record_counter = 0;
    size_t error_counter = 0;
    
    while ( ! m_InStream.eof() ) {

        NcbiGetlineEOL( m_InStream, line );
        ++line_counter;
        
        if ( IsHeaderLine( line ) || IsIgnoredLine( line ) ) {
            continue;
        }
        ++record_counter;
        
        CMaskData mask_data;
        if ( ! ParseRecord( line, mask_data ) ) {
            ++error_counter;
            LOG_POST_X( 1, Error << "Rmo Reader: Parse error in record " 
                << record_counter << " (line " << line_counter 
                << "). Record skipped" );
            if ( error_counter < MAX_ERROR_COUNT ) {
                continue;
            }
            else {
                break;
            }
        }
        
        if ( ! VerifyData( mask_data ) ) {
            ++error_counter;
            LOG_POST_X( 2, Error << "Rmo Reader: Verification error in record " 
                << record_counter << " (line " << line_counter 
                << "). Record skipped." );
            if ( error_counter < MAX_ERROR_COUNT ) {
                continue;
            }
            else {
                break;
            }
        }
        
        if ( ! MakeFeature( mask_data, feat, flags ) ) {
            // we don't tolerate even a few errors here!
            error_counter = MAX_ERROR_COUNT;
            LOG_POST_X( 3, Error << "Rmo Reader: Unable to create feature table for record " 
                << record_counter << " (line " << line_counter 
                << "). Aborting file import." );
            break;
        }
        
        ftable.push_back( feat );
    }
    
    if ( error_counter == MAX_ERROR_COUNT ) {
        LOG_POST_X( 4, Error << "Rmo Reader: File import aborted due to error count or severity." );
        throw 0; // upper layer catches everything in sight and reports error to file_loader.
    }
}


bool CRmOutReader::IsHeaderLine( const string& line )
{
    string labels_1st_line[] = { "SW", "perc", "query", "position", "matching", "" };
    string labels_2nd_line[] = { "score", "div.", "del.", "ins.", "sequence", "" };

    // try to identify 1st line of column labels:
    size_t current_offset = 0;
    size_t i = 0;
    for ( ; labels_1st_line[i] != ""; ++i ) {
        current_offset = NStr::FindCase( line, labels_1st_line[i], current_offset );
        if ( NPOS == current_offset ) {
            break;
        }
    }
    if ( labels_1st_line[i] == "" ) {
        return true;
    }
    
    // try to identify 2nd line of column labels:
    current_offset = 0;
    i = 0;
    for ( ; labels_2nd_line[i] != ""; ++i ) {
        current_offset = NStr::FindCase( line, labels_2nd_line[i], current_offset );
        if ( NPOS == current_offset ) {
            return false;
        }
    }
    return true;
}


bool CRmOutReader::IsIgnoredLine( const string& line )
{
    //
    //  Currently, only lines with only whitespace on them are ignored.
    //
    return ( NStr::TruncateSpaces( line ).length() == 0 );
}


bool CRmOutReader::ParseRecord( const string& record, CMaskData& mask_data )
{
    const size_t MIN_VALUE_COUNT = 15;
    
    string line = NStr::TruncateSpaces( record );
    list< string > values;
    if ( NStr::Split( line, " \t", values ).size() < MIN_VALUE_COUNT ) {
        return false;
    }
    
    try {
        // 1: "SW score"
        list<string>::iterator it = values.begin();
        mask_data.sw_score = NStr::StringToUInt( *it );
        
        // 2: "perc div."
        ++it;
        mask_data.perc_div = NStr::StringToDouble( *it );
        
        // 3: "perc del."
        ++it;
        mask_data.perc_del = NStr::StringToDouble( *it );
        
        // 4: "perc ins."
        ++it;
        mask_data.perc_ins = NStr::StringToDouble( *it );
        
        // 5: "query sequence"
        ++it;
        mask_data.query_sequence = *it;
        
        // 6: "position begin"
        ++it;
        mask_data.outer_pos_begin = NStr::StringToUInt( *it );
        
        // 7: "in end"
        ++it;
        mask_data.outer_pos_end = NStr::StringToUInt( *it );
        
        // 8: "query (left)"
        ++it;
        /* not used */
        
        // 9: "" (meaning "strand")
        ++it;
        mask_data.strand = *it;
        
        // 10: "matching repeat"
        ++it;
        mask_data.matching_repeat = *it;
        
        // 11: "repeat class/family"
        ++it;
        mask_data.repeat_class_family = *it;
        
        // 12: "position in"
        ++it;
        /* not used */
        
        // 13: "in end"
        ++it;
        /* not used */
        
        // 14: "repeat left"
        ++it;
        /* not used */
        
        // 15: "ID"
        ++it;
        /* not used */
        
    }
    catch( ... ) {
        return false;
    }
    
    return true;
}


bool CRmOutReader::VerifyData( const CMaskData& mask_data )
{
    //
    //  This would be the place for any higher level checks of the mask data
    //  collected from the record ...
    // 
    return true;
}


bool CRmOutReader::MakeFeature( const CMaskData& mask_data, CRef<CSeq_feat>& feat,
                                TFlags flags )
{
    feat.Reset( new CSeq_feat );
    feat->ResetLocation();
    
    //  data:
    CSeqFeatData& sfdata = feat->SetData();
    CImp_feat_Base& imp = sfdata.SetImp();
    imp.SetKey( "repeat_region" );
    
    //  location:
    CRef<CSeq_loc> location( new CSeq_loc );
    CSeq_interval& interval = location->SetInt();
    interval.SetFrom( min( mask_data.outer_pos_begin, mask_data.outer_pos_end ) -1 );
    interval.SetTo( max( mask_data.outer_pos_begin, mask_data.outer_pos_end ) -1 );
    interval.SetStrand( strcmp( mask_data.strand.c_str(), "C" ) ? 
        eNa_strand_plus : eNa_strand_minus );

    CBioseq::TId ids;
    CSeq_id::ParseFastaIds(ids, mask_data.query_sequence);
    location->SetId(*FindBestChoice(ids, CSeq_id::Score));

    feat->SetLocation( *location );

    //  qualifiers:
    if (flags) {
        CSeq_feat::TQual& qual_list = feat->SetQual();
        
        if (flags & fIncludeRepeatName) {
            CRef<CGb_qual> repeat( new CGb_qual );
            repeat->SetQual( "repeat_region" );
            repeat->SetVal( mask_data.matching_repeat );
            qual_list.push_back( repeat );
        }

        if (flags & fIncludeRepeatClass) {
            CRef<CGb_qual> rpt_family( new CGb_qual );
            rpt_family->SetQual( "rpt_family" );
            rpt_family->SetVal( mask_data.repeat_class_family );
            qual_list.push_back( rpt_family );
        }

        if (flags & fIncludeStatistics) {
            CRef<CGb_qual> sw_score( new CGb_qual );
            sw_score->SetQual( "sw_score" );
            sw_score->SetVal( NStr::IntToString( mask_data.sw_score ) );
            qual_list.push_back( sw_score );
            
            CRef<CGb_qual> perc_div( new CGb_qual );
            perc_div->SetQual( "perc_div" );
            perc_div->SetVal( NStr::DoubleToString( mask_data.perc_div ) );
            qual_list.push_back( perc_div );
            
            CRef<CGb_qual> perc_del( new CGb_qual );
            perc_del->SetQual( "perc_del" );
            perc_del->SetVal( NStr::DoubleToString( mask_data.perc_del ) );
            qual_list.push_back( perc_del );
            
            CRef<CGb_qual> perc_ins( new CGb_qual );
            perc_ins->SetQual( "perc_ins" );
            perc_ins->SetVal( NStr::DoubleToString( mask_data.perc_ins ) );
            qual_list.push_back( perc_ins );
        }
    }
    
    return true;
}


CRmReader* CRmReader::OpenReader( CNcbiIstream& InStream )
{
    //
    //  This is the point to make sure we are dealing with the right file type and
    //  to allocate the specialist reader for any subtype (OUT, HTML) we encouter.
    //  When this function returns the file pointer should be past the file header
    //  and at the beginning of the actual mask data.
    //
    //  Note:
    //  If something goes wrong during header processing then the file pointer will
    //  still be modified. It's the caller's job to restore the file pointer if this
    //  is possible for this type of stream.
    //
    
    //
    //  2006-03-31: Only supported file type at this time: ReadMasker OUT.
    //
    return new CRmOutReader( InStream );
}


void CRmReader::CloseReader( CRmReader* pReader )
{
    delete pReader;
}


END_objects_SCOPE
END_NCBI_SCOPE
