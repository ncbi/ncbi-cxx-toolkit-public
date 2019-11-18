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
#include <objects/general/Object_id.hpp>

#include <objects/seq/Seq_annot.hpp>

#include <objtools/readers/read_util.hpp>
#include <objtools/readers/reader_exception.hpp>
#include <objtools/readers/line_error.hpp>
#include <objtools/readers/reader_message.hpp>
#include <objtools/readers/reader_listener.hpp>
#include <objtools/readers/message_listener.hpp>
#include <objtools/readers/psl_reader.hpp>
#include <objtools/error_codes.hpp>

#include "psl_data.hpp"
#include "reader_message_handler.hpp"

#define NCBI_USE_ERRCODE_X   Objtools_Rd_RepMask

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
CPslReader::CPslReader(
    TReaderFlags flags,
    const string& name,
    const string& title,
    SeqIdResolver seqResolver,
    CReaderListener* pListener) :
//  ----------------------------------------------------------------------------
    CReaderBase(flags, name, title, seqResolver)
{
    m_pMessageHandler = new CReaderMessageHandler(pListener);
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
    ILineErrorListener* pEL) 
//  ----------------------------------------------------------------------------                
{
    xProgressInit(lineReader);

    CRef<CSeq_annot> pAnnot = xCreateSeqAnnot();
    auto& annotData = pAnnot->SetData();

    TReaderData readerData;
    xGetData(lineReader, readerData);
    while (!readerData.empty()) {
        try {
            xProcessData(readerData, annotData);
        }
        catch (CReaderMessage& err) {
            if (err.Severity() == eDiag_Fatal) {
                throw;
            }
            m_pMessageHandler->Report(err);
        }
        catch (ILineError& err) {
            if (!pEL  ||  err.GetSeverity() == eDiag_Fatal) {
                throw;
            }
            pEL->PutMessage(err);
        }
        catch (CException& err) {
            CReaderMessage terminator(
                eDiag_Fatal, 
                m_uLineNumber,
                "Exception: " + err.GetMsg());
            throw(terminator);
        }
        xGetData(lineReader, readerData);
    }
    return pAnnot;
}

//  ----------------------------------------------------------------------------
CRef<CSerialObject>
CPslReader::ReadObject(
    ILineReader& lr,
    ILineErrorListener* pMessageListener ) 
//  ----------------------------------------------------------------------------
{ 
    CRef<CSerialObject> object( 
        ReadSeqAnnot( lr, pMessageListener ).ReleaseOrNull() );    
    return object;
}

//  ----------------------------------------------------------------------------
CRef<CSeq_annot>
CPslReader::xCreateSeqAnnot() 
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_annot> pAnnot(new CSeq_annot);
    if (!m_AnnotName.empty()) {
        pAnnot->SetNameDesc(m_AnnotName);
    }
    if (!m_AnnotTitle.empty()) {
        pAnnot->SetTitleDesc(m_AnnotTitle);
    }
    return pAnnot;
}

//  ----------------------------------------------------------------------------
void
CPslReader::xGetData(
    ILineReader& lr,
    TReaderData& readerData)
//  ----------------------------------------------------------------------------
{
    readerData.clear();
    string line;
    if (xGetLine(lr, line)) {
        readerData.push_back(TReaderLine{m_uLineNumber, line});
    }
}

//  ----------------------------------------------------------------------------
void
CPslReader::xProcessData(
    const TReaderData& readerData,
    CSeq_annot::TData& annotData) 
//  ----------------------------------------------------------------------------
{
    CPslData pslData(m_pMessageHandler);
    for (auto line: readerData) {
        CRef<CSeq_align> pSeqAlign(new CSeq_align);
        pslData.Initialize(line);
        pslData.ExportToSeqAlign(mSeqIdResolve, *pSeqAlign);
        annotData.SetAlign().push_back(pSeqAlign);
    }
}

END_objects_SCOPE
END_NCBI_SCOPE
