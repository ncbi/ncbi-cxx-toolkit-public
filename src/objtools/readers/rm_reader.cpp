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

#include <algorithm>

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

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
    virtual void Read( CRef<CSeq_annot> );

    //
    //  data:
    //
    static const unsigned long BUFFERSIZE = 256;
    char pReadBuffer[ BUFFERSIZE ];
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


void CRmOutReader::Read( CRef<CSeq_annot> entry )
{
    string lineBuffer;
    CSeq_annot::C_Data::TFtable& ftable = entry->SetData().SetFtable();
    CRef<CSeq_feat> feat;
        
    NcbiGetlineEOL( m_InStream, lineBuffer );
    while ( ! lineBuffer.empty() ) {

        //
        //  Parse the RMO record:
        //
        istrstream MaskData( lineBuffer.c_str(), (streamsize)lineBuffer.length() );
    
        const unsigned long STRINGBUFFERSIZE = 32;
        
        unsigned long swScore, outerPosBegin, outerPosEnd, innerPosBegin,
            innerPosEnd;
        double percDiv, percDel, percIns;
        char querySequence[ STRINGBUFFERSIZE ], 
            strand[ STRINGBUFFERSIZE ],
            matchingRepeat[ STRINGBUFFERSIZE ], 
            repeatClassFamily[ STRINGBUFFERSIZE ];
        char tempBuffer[ STRINGBUFFERSIZE ];
    
        MaskData >> swScore;
        MaskData >> percDiv;
        MaskData >> percDel;
        MaskData >> percIns;
        MaskData.width( STRINGBUFFERSIZE ); MaskData >> querySequence;
        MaskData >> outerPosBegin;
        MaskData >> outerPosEnd;
        MaskData.width( STRINGBUFFERSIZE ); MaskData >> tempBuffer;
        MaskData.width( STRINGBUFFERSIZE ); MaskData >> strand;
        MaskData.width( STRINGBUFFERSIZE ); MaskData >> matchingRepeat;
        MaskData.width( STRINGBUFFERSIZE ); MaskData >> repeatClassFamily;
        
        MaskData.width( STRINGBUFFERSIZE ); MaskData >> tempBuffer;
        char* valueStart = tempBuffer;
        if ( *valueStart == '(' ) {
            ++valueStart;
        }
        innerPosBegin = atoi( valueStart );
    
        MaskData.width( STRINGBUFFERSIZE ); MaskData >> tempBuffer;
        valueStart = tempBuffer;
        if ( *valueStart == '(' ) {
            ++valueStart;
        }
        innerPosEnd = atoi( valueStart );
    
    
        //
        //  Basic sanity check on the data we got...
        //
        if ( MaskData.bad() ) {
            NCBI_THROW( CException, eUnknown, "Not enough data on the line." );
        }
            // ... and probably some restrictions on what can be contained in the 
            // parsed data.
            // TODO: Add extra checks once we get a grip on the common error patterns.
            
        
        //
        //  Use parse data to create another feature:
        //
        feat.Reset( new CSeq_feat );
        feat->ResetLocation();
        
        //  data
        CSeqFeatData& sfdata = feat->SetData();
        CImp_feat_Base& imp = sfdata.SetImp();
        imp.SetKey ( "repeat_region" );
        
        //  location
        CRef<CSeq_loc> location (new CSeq_loc);
        CSeq_interval& interval = location->SetInt();
        interval.SetFrom( outerPosBegin + innerPosBegin );
        interval.SetTo( outerPosBegin + innerPosEnd );
        interval.SetStrand( (0 == strcmp( strand, "C" )) ? eNa_strand_minus : eNa_strand_plus );
        CSeq_id seqId( querySequence );
        interval.SetId().Assign( seqId );
        feat->SetLocation (*location);
        
        //  qualifiers
        CSeq_feat::TQual& qual_list = feat->SetQual();
         
        CRef<CGb_qual> repeat( new CGb_qual );
        repeat->SetQual( "repeat_region" );
        repeat->SetVal( matchingRepeat );
        qual_list.push_back( repeat );
               
        CRef<CGb_qual> rpt_family( new CGb_qual );
        rpt_family->SetQual( "rpt_family" );
        rpt_family->SetVal( repeatClassFamily );
        qual_list.push_back( rpt_family );
               
        CRef<CGb_qual> sw_score( new CGb_qual );
        sw_score->SetQual( "sw_score" );
        sw_score->SetVal( NStr::IntToString(swScore) );
        qual_list.push_back( sw_score );
               
        CRef<CGb_qual> perc_div( new CGb_qual );
        perc_div->SetQual( "perc_div" );
        perc_div->SetVal( NStr::DoubleToString(percDiv) );
        qual_list.push_back( perc_div );
               
        CRef<CGb_qual> perc_del( new CGb_qual );
        perc_del->SetQual( "perc_del" );
        perc_del->SetVal( NStr::DoubleToString(percDel) );
        qual_list.push_back( perc_del );
               
        CRef<CGb_qual> perc_ins( new CGb_qual );
        perc_ins->SetQual( "perc_ins" );
        perc_ins->SetVal( NStr::DoubleToString(percIns) );
        qual_list.push_back( perc_ins );
               
        ftable.push_back( feat );
        
        //
        //  Get the next record:
        //
        NcbiGetlineEOL( m_InStream, lineBuffer );
    }
}


CRmReader* CRmReader::OpenReader( CNcbiIstream& InStream )
{
    //
    //  This is the point to make sure we are dealing with the right file ytpe and
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
    string line1, line2, line3;
    if ( NcbiGetlineEOL( InStream, line1 ) && NcbiGetlineEOL( InStream, line2 ) 
      && NcbiGetlineEOL( InStream, line3 ) ) {
        if ( string::npos != line1.find( "SW" ) && string::npos != line1.find( "perc" )
          && string::npos != line2.find( "div." ) && string::npos != line2.find( "del." )
          && string::npos != line2.find( "ins." ) && line3.empty() ) {
                  
            return new CRmOutReader( InStream );
        }
    }
    return 0;
}


void CRmReader::CloseReader( CRmReader* pReader )
{
    delete pReader;
}


END_objects_SCOPE
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2006/04/18 16:05:19  ucko
 * Use NStr::{Int,Double}ToString rather than printf, which might not
 * have been declared.
 *
 * Revision 1.1  2006/04/18 11:36:35  ludwigf
 * INIT: Implementation of the RepeatMasker OUT file reader.
 *
 *
 * ===========================================================================
 */
