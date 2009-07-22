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
 *   Basic reader interface.
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

#include <objects/seqset/Seq_entry.hpp>
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
#include <objects/seqfeat/Feat_id.hpp>

#include <objtools/readers/reader_exception.hpp>
#include <objtools/readers/line_error.hpp>
#include <objtools/readers/error_container.hpp>
#include <objtools/readers/reader_base.hpp>
#include <objtools/readers/bed_reader.hpp>
#include <objtools/readers/microarray_reader.hpp>
#include <objtools/readers/wiggle_reader.hpp>
#include <objtools/readers/gff3_reader.hpp>
#include <objtools/error_codes.hpp>

#include <algorithm>


#define NCBI_USE_ERRCODE_X   Objtools_Rd_RepMask

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
CReaderBase*
CReaderBase::GetReader(
    CFormatGuess::EFormat format,
    int flags )
//  ----------------------------------------------------------------------------
{
    switch ( format ) {
    default:
        return 0;
    case CFormatGuess::eBed:
        return new CBedReader( flags );
    case CFormatGuess::eBed15:
        return new CMicroArrayReader( flags );
    case CFormatGuess::eWiggle:
        return new CWiggleReader( flags );
    case CFormatGuess::eGtf:
        return new CGff3Reader( flags );
    }
}

//  ----------------------------------------------------------------------------
CRef< CSerialObject >
CReaderBase::ReadObject(
    CNcbiIstream& istr,
    IErrorContainer* pErrorContainer ) 
//  ----------------------------------------------------------------------------
{
    CStreamLineReader lr( istr );
    return ReadObject( lr, pErrorContainer );
}

//  ----------------------------------------------------------------------------
CRef< CSeq_annot >
CReaderBase::ReadSeqAnnot(
    CNcbiIstream& istr,
    IErrorContainer* pErrorContainer ) 
//  ----------------------------------------------------------------------------
{
    CStreamLineReader lr( istr );
    return ReadSeqAnnot( lr, pErrorContainer );
}

//  ----------------------------------------------------------------------------
CRef< CSeq_annot >
CReaderBase::ReadSeqAnnot(
    ILineReader&,
    IErrorContainer* ) 
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_annot> object( new CSeq_annot() );
    return object;
}
                
//  ----------------------------------------------------------------------------
CRef< CSeq_entry >
CReaderBase::ReadSeqEntry(
    CNcbiIstream& istr,
    IErrorContainer* pErrorContainer ) 
//  ----------------------------------------------------------------------------
{
    CStreamLineReader lr( istr );
    return ReadSeqEntry( lr, pErrorContainer );
}

//  ----------------------------------------------------------------------------
CRef< CSeq_entry >
CReaderBase::ReadSeqEntry(
    ILineReader&,
    IErrorContainer* ) 
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_entry> object( new CSeq_entry() );
    return object;
}
                
//  ----------------------------------------------------------------------------
bool CReaderBase::SplitLines( 
    const char* pcBuffer, 
    size_t uSize,
    list<string>& lines )
//  ----------------------------------------------------------------------------
{
    //
    //  Make sure the given data is ASCII before checking potential line breaks:
    //
    const size_t MIN_HIGH_RATIO = 20;
    size_t high_count = 0;
    for ( size_t i=0; i < uSize; ++i ) {
        if ( 0x80 & pcBuffer[i] ) {
            ++high_count;
        }
    }
    if ( 0 < high_count && uSize / high_count < MIN_HIGH_RATIO ) {
        return false;
    }

    //
    //  Let's expect at least one line break in the given data:
    //
    string data( pcBuffer, uSize );
    
    lines.clear();
    if ( NStr::Split( data, "\r\n", lines ).size() > 1 ) {
        return true;
    }
    lines.clear();
    if ( NStr::Split( data, "\r", lines ).size() > 1 ) {
        return true;
    }
    lines.clear();
    if ( NStr::Split( data, "\n", lines ).size() > 1 ) {
        return true;
    }
    
    //
    //  Suspicious for non-binary files. Unfortunately, it can actually happen
    //  for Newick tree files.
    //
    lines.clear();
    lines.push_back( data );
    return true;
}

//  ----------------------------------------------------------------------------
void
CReaderBase::ProcessError(
    CObjReaderLineException& err,
    IErrorContainer* pContainer )
//  ----------------------------------------------------------------------------
{
    err.SetLineNumber( m_uLineNumber );
    if ( 0 == pContainer ) {
        throw( err );
    }
    if ( ! pContainer->PutError( err ) )
    {
        throw( err );
    }
}

END_objects_SCOPE
END_NCBI_SCOPE
