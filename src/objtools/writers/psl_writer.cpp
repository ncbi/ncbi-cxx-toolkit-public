
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
 * Authors:  Frank Ludwig
 *
 * File Description:  Write alignment
 *
 */

#include <ncbi_pch.hpp>

#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Spliced_exon.hpp>
#include <objects/seqalign/Sparse_seg.hpp>
#include <objects/seqalign/Sparse_align.hpp>
#include <objects/seqalign/Product_pos.hpp>
#include <objects/seqalign/Prot_pos.hpp>
#include <objects/seqalign/Spliced_exon_chunk.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seq/seq_id_handle.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>

#include <objtools/writers/writer_listener.hpp>
#include <objtools/writers/writer_message.hpp>
#include <objtools/writers/writer_exception.hpp>
#include <objtools/writers/write_util.hpp>
#include <objtools/writers/psl_writer.hpp>
#include "psl_record.hpp"
#include "psl_formatter.hpp"

#include <util/sequtil/sequtil_manip.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ----------------------------------------------------------------------------
CPslWriter::CPslWriter(
    CScope& scope,
    CNcbiOstream& ostr,
    unsigned int uFlags) :
//  ----------------------------------------------------------------------------
    CWriterBase(ostr, uFlags),
    mRecordCounter(0)
{
    m_pScope.Reset(&scope);
};


//  ----------------------------------------------------------------------------
CPslWriter::CPslWriter(
    CNcbiOstream& ostr,
    unsigned int uFlags) :
    CPslWriter(*(new CScope(*CObjectManager::GetInstance())), ostr, uFlags)
//  ----------------------------------------------------------------------------
{
};


//  ----------------------------------------------------------------------------
bool CPslWriter::WriteAnnot(
    const CSeq_annot& annot,
    const string& name,
    const string& descr)
//  ----------------------------------------------------------------------------
{
    if (!annot.IsAlign()) {
        return CWriterBase::WriteAnnot(annot, name, descr);
    }
    for (const auto& pAlign: annot.GetData().GetAlign()) {
        if (!WriteAlign(*pAlign, name, descr)) {
            return false;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CPslWriter::WriteAlign(
    const CSeq_align& align,
    const string& name,
    const string& descr)
//  ----------------------------------------------------------------------------
{
    if (IsCanceled()) {
        NCBI_THROW(
            CObjWriterException,
            eInterrupted,
            "Processing terminated by user");
    }

    ++mRecordCounter;
    if (m_uFlags & CPslWriter::fDebugOutput) {
        cerr << ".";
        if (0 == mRecordCounter % 50) {
            cerr << " " << mRecordCounter << endl;
        }
    }

    try {
        CPslRecord record(mpMessageListener);
        auto segType = align.GetSegs().Which();
        switch (segType) {

        default:
            throw CWriterMessage(
                "Input alignment type not supported", eDiag_Error);

        case CSeq_align::C_Segs::e_Spliced:
            record.Initialize(*m_pScope, align.GetSegs().GetSpliced());
            break;

        case CSeq_align::C_Segs::e_Denseg:
            record.Initialize(*m_pScope, align.GetSegs().GetDenseg());
            if (align.CanGetScore()) {
                record.Initialize(*m_pScope, align.GetScore());
            }
            break;

        case CSeq_align::C_Segs::e_Disc:
            const auto& data = align.GetSegs().GetDisc().Get();
            for (const auto& pAlign: data) {
                WriteAlign(*pAlign);
            }
            return true;
        }
        record.Finalize();

        CPslFormatter formatter(m_Os, m_uFlags);
        formatter.Format(record);
    }
    catch(CWriterMessage& writerMessage) {
        switch(writerMessage.GetSeverity()) {
            case eDiag_Fatal:
                throw;
            default:
                PutMessage(writerMessage);
                break;
        }
    }
    catch(CException& e) {
        throw CWriterMessage("Exception thrown: " + e.GetMsg(), eDiag_Error);
    }
    return true;
}

END_NCBI_SCOPE

