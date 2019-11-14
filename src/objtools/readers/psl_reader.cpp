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
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Score.hpp>

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
    auto& alignData = pAnnot->SetData().SetAlign();

    string line;
    CPslData pslData;
    CRef<CSeq_align> pAlign;
    while (xGetLine(lineReader, line)) {
        CRef<CSeq_align> pAlign(new CSeq_align);
        pslData.Initialize(line);
        //pslData.Dump(cerr);
        alignData.push_back(xCreateSeqAlign(pslData));
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
CRef<CSeq_align>
CPslReader::xCreateSeqAlign(
    const CPslData& pslData)
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_align> pSeqAlign(new CSeq_align);
    
    pSeqAlign->SetType(CSeq_align::eType_partial);
    CDense_seg& denseSeg = pSeqAlign->SetSegs().SetDenseg();

    auto& ids = denseSeg.SetIds();
    ids.push_back(CReadUtil::AsSeqId(pslData.NameQ()));
    ids.push_back(CReadUtil::AsSeqId(pslData.NameT()));
    
    vector<SAlignSegment> segments;
    pslData.ConvertBlocksToSegments(segments);
    for (const auto& segment: segments) {
        denseSeg.SetLens().push_back(segment.mLen);
        denseSeg.SetStarts().push_back(segment.mStartQ);
        denseSeg.SetStarts().push_back(segment.mStartT);
        denseSeg.SetStrands().push_back(segment.mStrandQ);
        denseSeg.SetStrands().push_back(segment.mStrandT);
    }
    denseSeg.SetNumseg(segments.size());

    
    CRef<CScore>  pMatches(new CScore);
    pMatches->SetId().SetStr("num_match");
    pMatches->SetValue().SetInt(pslData.Matches());
    denseSeg.SetScores().push_back(pMatches);
    CRef<CScore>  pMisMatches(new CScore);
    pMisMatches->SetId().SetStr("num_mismatch");
    pMisMatches->SetValue().SetInt(pslData.MisMatches());
    denseSeg.SetScores().push_back(pMisMatches);
    CRef<CScore> pRepMatches(new CScore);
    pRepMatches->SetId().SetStr("num_repmatch");
    pRepMatches->SetValue().SetInt(pslData.RepMatches());
    denseSeg.SetScores().push_back(pRepMatches);
    CRef<CScore> pCountN(new CScore);
    pCountN->SetId().SetStr("num_n");
    pCountN->SetValue().SetInt(pslData.CountN());
    denseSeg.SetScores().push_back(pCountN);
    return pSeqAlign;
}

END_objects_SCOPE
END_NCBI_SCOPE
