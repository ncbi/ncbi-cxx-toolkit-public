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
#include <objects/seqset/Bioseq_set.hpp>

#include <objtools/readers/reader_exception.hpp>
#include <objtools/readers/line_error.hpp>
#include <objtools/readers/error_container.hpp>
#include <objtools/readers/gff3_reader.hpp>
#include <objtools/error_codes.hpp>

#include <algorithm>


#define NCBI_USE_ERRCODE_X   Objtools_Rd_RepMask

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
CGff3Reader::CGff3Reader(
    int iFlags ):
//  ----------------------------------------------------------------------------
    m_iReaderFlags( iFlags ),
    m_pErrors( 0 )
{
}

//  ----------------------------------------------------------------------------
CGff3Reader::~CGff3Reader()
//  ----------------------------------------------------------------------------
{
}


//  ----------------------------------------------------------------------------
bool CGff3Reader::VerifyLine(
    const string& line )
//  ----------------------------------------------------------------------------
{
    vector<string> tokens;
    if ( NStr::Tokenize( line, " \t", tokens, NStr::eMergeDelims ).size() < 8 ) {
        return false;
    }
    try {
        NStr::StringToInt( tokens[3] );
        NStr::StringToInt( tokens[4] );
        if ( tokens[5] != "." ) {
            NStr::StringToDouble( tokens[5] );
        }
    }
    catch( ... ) {
        return false;
    }
        
    if ( tokens[6] != "+" && tokens[6] != "." && tokens[6] != "-" ) {
        return false;
    }
    if ( tokens[6].size() != 1 || NPOS == tokens[6].find_first_of( ".+-" ) ) {
        return false;
    }
    if ( tokens[7].size() != 1 || NPOS == tokens[7].find_first_of( ".0123" ) ) {
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------                
CRef< CSeq_entry >
CGff3Reader::ReadSeqEntry(
    ILineReader& lr,
    IErrorContainer* pErrorContainer ) 
//  ----------------------------------------------------------------------------                
{ 
    string line;
    int linecount = 0;
    
    m_pErrors = pErrorContainer;
    CRef< CSeq_entry > entry = CGFFReader::Read( lr, m_iReaderFlags );
    m_pErrors = 0;
    return entry;
}
    
//  ----------------------------------------------------------------------------                
CRef< CSerialObject >
CGff3Reader::ReadObject(
    ILineReader& lr,
    IErrorContainer* pErrorContainer ) 
//  ----------------------------------------------------------------------------                
{ 
    string line;
    int linecount = 0;
    
    m_pErrors = pErrorContainer;
    CRef< CSeq_entry > entry = CGFFReader::Read( lr, m_iReaderFlags );
    m_pErrors = 0;
    CRef<CSerialObject> object( entry.ReleaseOrNull() );
    return object;
}
    
//  ----------------------------------------------------------------------------                
void 
CGff3Reader::x_Info(
    const string& message,
    unsigned int line )
//  ----------------------------------------------------------------------------                
{
    if ( !m_pErrors ) {
        return CGFFReader::x_Info( message, line );
    }
    CObjReaderLineException err( eDiag_Info, line, message );
    CReaderBase::m_uLineNumber = line;
    ProcessError( err, m_pErrors );
}

//  ----------------------------------------------------------------------------                
void 
CGff3Reader::x_Warn(
    const string& message,
    unsigned int line )
//  ----------------------------------------------------------------------------                
{
    if ( !m_pErrors ) {
        return CGFFReader::x_Warn( message, line );
    }
    CObjReaderLineException err( eDiag_Warning, line, message );
    CReaderBase::m_uLineNumber = line;
    ProcessError( err, m_pErrors );
}

//  ----------------------------------------------------------------------------                
void 
CGff3Reader::x_Error(
    const string& message,
    unsigned int line )
//  ----------------------------------------------------------------------------                
{
    if ( !m_pErrors ) {
        return CGFFReader::x_Error( message, line );
    }
    CObjReaderLineException err( eDiag_Error, line, message );
    CReaderBase::m_uLineNumber = line;
    ProcessError( err, m_pErrors );
}

END_objects_SCOPE
END_NCBI_SCOPE
