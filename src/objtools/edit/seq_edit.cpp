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
* Author: Justin Foley, NCBI
*
* File Description:
*   functions for editing sequences
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <objmgr/seq_vector.hpp>

#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/seqport_util.hpp>

#include <objtools/edit/seq_edit.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)

void g_ConvertDeltaToRawSeq(CBioseq& bioseq, CScope* pScope)
{
    auto& seq_inst = bioseq.SetInst();

    _ASSERT(seq_inst.GetRepr() == CSeq_inst::eRepr_delta);
    _ASSERT(seq_inst.IsSetLength());

    CSeqVector seq_vec(bioseq, pScope, CBioseq_Handle::eCoding_Iupac);
    string seqdata;
    seq_vec.GetSeqData(0, seq_inst.GetLength(), seqdata);
    auto pSeqData =
        Ref(new CSeq_data(seqdata, seq_vec.GetCoding()));
    CSeqportUtil::Pack(pSeqData.GetPointer());


    seq_inst.SetRepr(CSeq_inst::eRepr_raw);
    seq_inst.SetSeq_data(*pSeqData);
    seq_inst.ResetExt();
}

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE
