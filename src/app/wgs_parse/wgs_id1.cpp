/*
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
* Author:  Alexey Dobronadezhdin
*
* File Description:
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <objects/id1/id1_client.hpp>
#include <objects/seq/Seq_inst.hpp>

#include "wgs_id1.hpp"
#include "wgs_utils.hpp"

namespace wgsparse
{

CRef<CSeq_entry> GetMasterEntryById(const string& prefix, CSeq_id::E_Choice choice)
{
    CRef<CSeq_entry> ret;

    static const string suffix = "00000000";
    string accession = prefix + suffix;
    CTextseq_id text_id;
    text_id.SetAccession(accession);

    auto set_fun = FindSetTextSeqIdFunc(choice);
    if (set_fun == nullptr) {
        return ret;
    }

    CSeq_id id;
    (id.*set_fun)(text_id);

    CID1Client id1;

    TGi gi = 0;
    for (size_t i = 0; i < 3; ++i) {
        gi = max(gi, id1.AskGetgi(id));
        text_id.SetAccession().push_back('0');
    }

    if (gi > 0) {
        ret = id1.FetchEntry(gi);

        // should be a sequence of nucleotides
        if (!ret->IsSeq() || !ret->GetSeq().IsSetInst() || !ret->GetSeq().GetInst().IsNa()) {
            ret.Reset();
        }
    }

    return ret;
}

}