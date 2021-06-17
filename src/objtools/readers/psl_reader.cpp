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
 *   PSL file reader
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seqalign/Seq_align.hpp>

#include <objtools/readers/psl_reader.hpp>
#include "psl_data.hpp"

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
    CReaderBase(flags, name, title, seqResolver, pListener)
{
}


//  ----------------------------------------------------------------------------
CPslReader::~CPslReader()
//  ----------------------------------------------------------------------------
{
}

//  ----------------------------------------------------------------------------
void
CPslReader::xProcessData(
    const TReaderData& readerData,
    CSeq_annot& annot)
//  ----------------------------------------------------------------------------
{
    CPslData pslData(m_pMessageHandler.get());
    auto& aligns = annot.SetData().SetAlign();
    for (auto line: readerData) {
        CRef<CSeq_align> pSeqAlign(new CSeq_align);
        pslData.Initialize(line);
        pslData.ExportToSeqAlign(mSeqIdResolve, *pSeqAlign);
        aligns.push_back(pSeqAlign);
    }
}

END_objects_SCOPE
END_NCBI_SCOPE
