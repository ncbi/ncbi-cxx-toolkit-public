
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

#include <objtools/writers/writer_exception.hpp>
#include <objtools/writers/write_util.hpp>
#include <objtools/writers/psl_writer.hpp>
#include "psl_record.hpp"

#include <util/sequtil/sequtil_manip.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ----------------------------------------------------------------------------
CPslWriter::CPslWriter(
    CScope& scope,
    CNcbiOstream& ostr,
    unsigned int uFlags) :
    CWriterBase(ostr, uFlags) 
//  ----------------------------------------------------------------------------
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
bool CPslWriter::WriteAlign(
    const CSeq_align& align,
    const string& name,
    const string& descr) 
//  ----------------------------------------------------------------------------
{
    xWritePreamble();
    switch (align.GetSegs().Which()) {
    case CSeq_align::C_Segs::e_Spliced:
        xWriteAlignSlicedSeg(align.GetSegs().GetSpliced());
        return true;
    default:
        return false;
    }
}

//  ----------------------------------------------------------------------------
void CPslWriter::xWritePreamble()
//  ----------------------------------------------------------------------------
{
    m_Os << "!! The PSL writer is still under development!" << endl;
    m_Os << "!! It does not produce valid output." << endl;
    m_Os << "!! Please don't use it yet." << endl;
    m_Os << endl;
}

//  ----------------------------------------------------------------------------
void CPslWriter::xWriteAlignSlicedSeg(
    const CSpliced_seg& splicedSeg)
//  ----------------------------------------------------------------------------
{
    CPslRecord psl;
    psl.Initialize(splicedSeg);
    psl.Write(m_Os, true);
}

END_NCBI_SCOPE

