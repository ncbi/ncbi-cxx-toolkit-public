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
 *   BED file reader
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
#include <objects/seq/Seq_annot.hpp>

#include <objtools/readers/read_util.hpp>
#include <objtools/readers/reader_exception.hpp>
#include <objtools/readers/line_error.hpp>
#include <objtools/readers/message_listener.hpp>
#include <objtools/readers/psl_reader.hpp>
#include <objtools/error_codes.hpp>

#include "psl_data.hpp"

#define NCBI_USE_ERRCODE_X   Objtools_Rd_RepMask

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
CPslReader::CPslReader(
    int flags) :
//  ----------------------------------------------------------------------------
    CReaderBase(flags)
{
}


//  ----------------------------------------------------------------------------
CPslReader::~CPslReader()
//  ----------------------------------------------------------------------------
{
}

//  ----------------------------------------------------------------------------
CRef< CSeq_annot >
CPslReader::ReadSeqAnnot(
    CNcbiIstream& istr,
    ILineErrorListener* pMessageListener ) 
//  ----------------------------------------------------------------------------
{
    CStreamLineReader lr( istr );
    return ReadSeqAnnot( lr, pMessageListener );
}

//  ----------------------------------------------------------------------------                
CRef< CSeq_annot >
CPslReader::ReadSeqAnnot(
    ILineReader& lineReader,
    ILineErrorListener* pEC ) 
//  ----------------------------------------------------------------------------                
{
    xProgressInit(lineReader);

    CRef<CSeq_annot> pAnnot(new CSeq_annot);;
    pAnnot->SetData().SetAlign();

    string line;
    CPslData pslData;
    while (xGetLine(lineReader, line)) {
        //pslData.Initialize(line);
        //pslData.Dump(cerr);
    }
    return pAnnot;
}

//  ----------------------------------------------------------------------------                
CRef< CSerialObject >
CPslReader::ReadObject(
    ILineReader& lr,
    ILineErrorListener* pMessageListener ) 
//  ----------------------------------------------------------------------------                
{ 
    CRef<CSerialObject> object( 
        ReadSeqAnnot( lr, pMessageListener ).ReleaseOrNull() );    
    return object;
}


END_objects_SCOPE
END_NCBI_SCOPE
