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
* Authors:  Sergiy Gotvyanskyy, NCBI
*
* File Description:
*   Converts runs of 'N' or 'n' into a gap
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbiutil.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_gap.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/general/Int_fuzz.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_ext.hpp>

#include <util/sequtil/sequtil_convert.hpp>

#include <objtools/edit/gaps_edit.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

void  CGapsEditor::ConvertNs2Gaps(const CSeq_data& data, size_t len, CDelta_ext& ext, size_t gap_min)
{
    CSeqUtil::TCoding src_coding = CSeqUtil::e_not_set;
    CSeqUtil::TCoding dst_coding = CSeqUtil::e_Iupacna;
    string decoded;
    CTempString      src;
    switch (data.Which()) {
#define CODING_CASE(x) \
    case CSeq_data::e_##x: \
        src.assign(&data.Get##x().Get()[0], data.Get##x().Get().size()); \
        src_coding = CSeqUtil::e_##x; \
        CSeqConvert::Convert(src, src_coding, 0, len, decoded,  dst_coding); \
        break;
    CODING_CASE(Ncbi2na)
    CODING_CASE(Iupacna)
    CODING_CASE(Iupacaa)
    CODING_CASE(Ncbi4na)
    CODING_CASE(Ncbi8na)
    CODING_CASE(Ncbi8aa)
    CODING_CASE(Ncbieaa)
    CODING_CASE(Ncbistdaa)
#undef CODING_CASE
    default:
//        ERR_POST_X(1, Warning << "PackWithGaps: unsupported encoding "
//                      << CSeq_data::SelectionName(data.Which()));
        return;
    }

    {
        size_t index = 0;
        CTempString current(decoded);
        size_t start;
        while (index + gap_min <= current.length() && ((start = current.find_first_of("Nn", index)) != CTempString::npos))
        {
            size_t end = current.find_first_not_of("Nn", start);
            if (end == CTempString::npos)
                end = current.length();
            if (end - start >= gap_min)
            {
                if (start > 0)
                    ext.AddAndSplit(current, CSeq_data::e_Iupacna, start, false, true);

                CDelta_seq& gap = ext.AddLiteral(end-start);
                gap.SetLiteral().SetSeq_data().SetGap().SetType(CSeq_gap::eType_unknown);
                current.assign(current.data(), end, current.length() - end);
                end = 0;
            }
            index = end;
        }
        if (current.length() > 0)
            ext.AddAndSplit(current, CSeq_data::e_Iupacna, current.length(), false, true);
    }
}

void CGapsEditor::ConvertNs2Gaps(CBioseq::TInst& inst, size_t gap_min)
{
    if (inst.IsAa()  ||  !inst.IsSetSeq_data()  ||  inst.IsSetExt()) {
        return;
    }
    const CSeq_data& data = inst.GetSeq_data();

    CDelta_ext& ext = inst.SetExt().SetDelta();

    ConvertNs2Gaps(data, inst.GetLength(), ext, gap_min);

    if (ext.Get().size() > 1) { // finalize
        inst.SetRepr(CSeq_inst::eRepr_delta);
        inst.ResetSeq_data();
    } else { // roll back
        inst.ResetExt();
    }
}

void CGapsEditor::ConvertNs2Gaps(CBioseq& bioseq, 
    TSeqPos gapNmin, TSeqPos gap_Unknown_length, 
    CSeq_gap::EType gap_type,
    CLinkage_evidence::EType evidence)
{
    if (bioseq.IsSetInst() && bioseq.GetInst().IsSetSeq_data() && !bioseq.GetInst().GetSeq_data().IsGap())
    {
        ConvertNs2Gaps(bioseq.SetInst(), gapNmin);
    }


    if (!bioseq.IsSetInst() || !bioseq.GetInst().IsSetExt() || !bioseq.GetInst().GetExt().IsDelta())
    {
        return;
    }

    CSeq_inst& inst = bioseq.SetInst();

    // since delta functions allows adding new literal to an end we create a copy of literal array 
    // to iterate over

    CDelta_ext::Tdata src_data = inst.GetExt().GetDelta().Get();
    CDelta_ext& dst_data = inst.SetExt().SetDelta();
    dst_data.Set().clear();

    NON_CONST_ITERATE(CDelta_ext::Tdata, it, src_data)
    {
        if (!(**it).IsLiteral())
        {
            dst_data.Set().push_back(*it);
            continue;
        }

        CDelta_seq::TLiteral& lit = (**it).SetLiteral();

        // split a literal to elements if needed
        if (lit.IsSetSeq_data() && !lit.GetSeq_data().IsGap())
        {
            ConvertNs2Gaps(lit.GetSeq_data(), lit.GetLength(), dst_data, gapNmin);
            continue;
        }

        // otherwise add it as is and possible update some parameters
        dst_data.Set().push_back(*it);

        if (lit.IsSetLength() && lit.GetLength() == gap_Unknown_length)
        {
            lit.SetFuzz().SetLim(CInt_fuzz::eLim_unk);
        }
        if (evidence >= 0)
        {
            if (lit.IsSetSeq_data() && lit.GetSeq_data().IsGap() && lit.GetSeq_data().GetGap().GetLinkage_evidence().size() > 0)
                continue;

            CRef<CLinkage_evidence> le(new CLinkage_evidence);
            le->SetType(evidence);
            lit.SetSeq_data().SetGap().SetLinkage_evidence().push_back(le);
            lit.SetSeq_data().SetGap().SetType(gap_type);
        }
    }
}

void CGapsEditor::ConvertNs2Gaps(
    objects::CSeq_entry& entry, TSeqPos gapNmin, TSeqPos gap_Unknown_length,
    CSeq_gap::EType gap_type,
    CLinkage_evidence::EType evidence)
{
    if (gapNmin==0 && gap_Unknown_length > 0)
        return;

    switch(entry.Which())
    {
    case CSeq_entry::e_Seq:
        {
            ConvertNs2Gaps(entry.SetSeq(), gapNmin, gap_Unknown_length, gap_type, evidence);
        }
        break;
    case CSeq_entry::e_Set:
        NON_CONST_ITERATE(CSeq_entry::TSet::TSeq_set, it, entry.SetSet().SetSeq_set())
        {
            ConvertNs2Gaps(**it, gapNmin, gap_Unknown_length, gap_type, evidence);
        }
        break;
    default:
        break;
    }
}

END_SCOPE(objects)
END_NCBI_SCOPE
