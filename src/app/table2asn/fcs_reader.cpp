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
* Author:  Frank Ludwig, Sergiy Gotvyanskyy, NCBI
*
* File Description:
*   Reader for selected data file formats
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <objtools/readers/fasta.hpp>
#include <objtools/readers/idmapper.hpp>

#include <objects/seqset/Seq_entry.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seqloc/Seq_bond.hpp>
#include <objects/seq/seqport_util.hpp>
#include <util/sequtil/sequtil_convert.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Feat_id.hpp>

//#include <objtools/readers/error_container.hpp>

//#include "multireader.hpp"
//#include "table2asn_context.hpp"

#include "fcs_reader.hpp"
#include "table2asn_context.hpp"


#include <common/test_assert.h>  /* This header must go last */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

namespace
{
CSeqConvert::TCoding xGetCoding(CSeq_data::E_Choice ch)
{
    switch (ch)
    {
        case CSeq_data::e_Iupacna:   return CSeqUtil::e_Iupacna;
        case CSeq_data::e_Iupacaa:   return CSeqUtil::e_Iupacaa;
        case CSeq_data::e_Ncbi2na:   return CSeqUtil::e_Ncbi2na;
        case CSeq_data::e_Ncbi4na:   return CSeqUtil::e_Ncbi4na;
        case CSeq_data::e_Ncbi8na:   return CSeqUtil::e_Ncbi8na;
        //case CSeq_data::e_Ncbipna:   return CSeqUtil::e_Ncbipna;
        case CSeq_data::e_Ncbi8aa:   return CSeqUtil::e_Ncbi8aa;
        case CSeq_data::e_Ncbieaa:   return CSeqUtil::e_Ncbieaa;
        //case CSeq_data::e_Ncbipaa:   return CSeqUtil::e_Ncbipaa;
        case CSeq_data::e_Ncbistdaa: return CSeqUtil::e_Ncbistdaa;
        default:
            return CSeqUtil::e_not_set;
    }
}
}

CForeignContaminationScreenReportReader::CForeignContaminationScreenReportReader(const CTable2AsnContext& context):
 m_context(context)
{
}

CForeignContaminationScreenReportReader::~CForeignContaminationScreenReportReader()
{
}

void CForeignContaminationScreenReportReader::LoadFile(ILineReader& reader)
{
    while (!reader.AtEOF())
    {
        reader.ReadLine();
        // First line is a collumn definitions
        CTempString current = reader.GetCurrentLine();
        if (current.empty())
            continue;
        if (*current.begin() == '#')
            continue;

        // Each line except first is a set of values, first collumn is a sequence id
        vector<string> values;
        NStr::Tokenize(current, "\t", values);
        if (values.size() == 5)
        {
            TColumns& new_cols = m_data["lcl|" + values[0]];
            new_cols.name = values[0];
            new_cols.length = NStr::StringToInt(values[1]);
            if (values[2] != "-")
            {
                string s1, s2;
                NStr::SplitInTwo(values[2], "..", s1, s2, NStr::fSplit_ByPattern);
                int start = NStr::StringToInt(s1) - 1;
                int len   = NStr::StringToInt(s2) - start;
                new_cols.locs[start] = len;
            }
            new_cols.mode  = values[3][0];
            new_cols.source = values[4];
        }
    }
}

void CForeignContaminationScreenReportReader::xTrimData(CSeq_inst& inst, const Tlocs& locs) const
{
    if (!inst.IsSetSeq_data())
        return;
    int removed = 0;

    string strdata(inst.GetSeq_data().GetIupacna().Get());
    ITERATE(Tlocs, it, locs)
    {
        strdata.erase(it->first - removed, it->second);
        removed += it->second;
    }

    inst.SetSeq_data().SetIupacna().Set(strdata);
    inst.SetLength(strdata.length());
}

void CForeignContaminationScreenReportReader::xTrimLiteral(objects::CSeq_literal& lit, int start, int stop) const
{
    if (lit.IsSetSeq_data() && !lit.GetSeq_data().IsGap())
    {
        string* encoded_str = 0;
        vector< char >* encoded_vec = 0;
        CSeqUtil::TCoding src_coding = xGetCoding(lit.GetSeq_data().Which());
        switch (lit.GetSeq_data().Which())
        {
        case CSeq_data::e_Iupacna:
            encoded_str = &lit.SetSeq_data().SetIupacna().Set();
            break;
        case CSeq_data::e_Iupacaa:
            encoded_str = &lit.SetSeq_data().SetIupacaa().Set();
            break;
        case CSeq_data::e_Ncbi2na:
            encoded_vec = &lit.SetSeq_data().SetNcbi2na().Set();
            break;
        case CSeq_data::e_Ncbi4na:
            encoded_vec = &lit.SetSeq_data().SetNcbi4na().Set();
            break;
        case CSeq_data::e_Ncbi8na:
            encoded_vec = &lit.SetSeq_data().SetNcbi8na().Set();
            break;
        case CSeq_data::e_Ncbipna:
            encoded_vec = &lit.SetSeq_data().SetNcbipna().Set();
            break;
        case CSeq_data::e_Ncbi8aa:
            encoded_vec = &lit.SetSeq_data().SetNcbi8aa().Set();
            break;
        case CSeq_data::e_Ncbieaa:
            encoded_str = &lit.SetSeq_data().SetNcbieaa().Set();
            break;
        case CSeq_data::e_Ncbipaa:
            encoded_vec = &lit.SetSeq_data().SetNcbipaa().Set();
            break;
        case CSeq_data::e_Ncbistdaa:
            encoded_vec = &lit.SetSeq_data().SetNcbistdaa().Set();
            break;
        default:
            return;
        }
        string decoded;
        if (encoded_vec)
        {
            CSeqConvert::Convert(*encoded_vec, src_coding, 0, lit.GetLength(), decoded,  CSeqUtil::e_Iupacna);
        }
        else
        if (encoded_str)
        {
            CSeqConvert::Convert(*encoded_str, src_coding, 0, lit.GetLength(), decoded,  CSeqUtil::e_Iupacna);
        }
        decoded.erase(start, stop-start+1);
        if (encoded_vec)
        {
            CSeqConvert::Convert(decoded, CSeqUtil::e_Iupacna, 0, decoded.length(), *encoded_vec, src_coding);
        }
        else
        if (encoded_str)
        {
            CSeqConvert::Convert(decoded, CSeqUtil::e_Iupacna, 0, decoded.length(), *encoded_str, src_coding);
        }
    }
    lit.SetLength() -= (stop-start+1);
}

void CForeignContaminationScreenReportReader::xTrimExt(CSeq_inst& inst, const Tlocs& locs) const
{
    if (!(inst.IsSetExt() && inst.GetExt().IsDelta()))
       return;

    CDelta_ext::Tdata& data = inst.SetExt().SetDelta().Set();
    int current_abs = 0;
    for (CDelta_ext::Tdata::iterator it = data.begin(); it != data.end();)
    {
        CDelta_seq& seq_data = **it;
        CDelta_ext::Tdata::iterator removable = it++;

        const int orig_lit_len = seq_data.GetLiteral().GetLength();
        int local_removed = 0;

        ITERATE(Tlocs, it_locs, locs)
        {
            const int start = it_locs->first;
            if (start < current_abs+orig_lit_len-local_removed &&
                current_abs < start+it_locs->second)
            {
               // found, lets trim
               int local_start = max(start-current_abs, 0);
               int local_end   = start+it_locs->second-local_removed-current_abs-1;
               local_end = min(local_end, (int)seq_data.GetLiteral().GetLength()-1);

               local_removed += (local_end-local_start+1);

               if (local_start == 0 && local_end == seq_data.GetLiteral().GetLength()-1)
               {
                   data.erase(removable);
                   break;
               }
               else
               {
                   xTrimLiteral(seq_data.SetLiteral(), local_start, local_end);
                   //cout << "Removing fragment:" << start+1 << ":" << start+it_locs->second << endl;
               }

            }
        }
        inst.SetLength() -= local_removed;
        current_abs += orig_lit_len;
    }

}

bool CForeignContaminationScreenReportReader::xCheckLen(const CBioseq& seq) const
{
    if (seq.IsSetLength())
    {
        if (seq.GetLength() < m_context.m_minimal_sequence_length)
        {
            return true;
        }
    }
    if (seq.IsSetInst() && seq.GetInst().IsSetExt() && seq.GetInst().GetExt().IsDelta())
    {
        ITERATE(CDelta_ext::Tdata, it, seq.GetInst().GetExt().GetDelta().Get())
        {
            const CDelta_seq& delta_seq = **it;
            if (delta_seq.IsLoc())
                return false;
            if (delta_seq.IsLiteral() && delta_seq.GetLiteral().IsSetSeq_data())
                return false;
        }
        return true;
    }

    return false;
}

// -r C:\Users\gotvyans\Desktop\cplusplus\results -i C:\Users\gotvyans\Desktop\cplusplus\data\test\x.fsa -s -fcs-file C:\Users\gotvyans\Desktop\cplusplus\data\test\FCSresults -fcs-trim -min-threshold 200 -a r10k
bool CForeignContaminationScreenReportReader::AnnotateOrRemove(CSeq_entry& entry) const
{
    string label;
    entry.GetLabel(&label, CSeq_entry::eContent);
    if (label.empty())
        return true;

    if (entry.IsSeq())
    {
        if (entry.GetSeq().GetId().empty())
        {
            return true;
        }
        if (entry.GetSeq().IsSetLength())
        {
            if (entry.GetSeq().GetLength() < m_context.m_minimal_sequence_length)
            {
                return true;
            }
        }
    }

    Tdata::const_iterator it = m_data.find(label);

    if (it == m_data.end())
    {
        return false;
    }

    {
        const TColumns& data = it->second;
        if (data.mode == 'X')
        {
            // remove it
            return true;
        }
        else
        if (data.mode == 'M')
        {
            if (m_context.m_fcs_trim && entry.GetSeq().IsSetInst())
            {
                if (entry.GetSeq().GetInst().IsSetExt())
                    xTrimExt(entry.SetSeq().SetInst(), data.locs);
                else
                    xTrimData(entry.SetSeq().SetInst(), data.locs);

                if (xCheckLen(entry.GetSeq()))
                    return true;
            }
            else
            ITERATE(Tlocs, it_loc, it->second.locs)
            {
                CRef<CSeq_feat> feat(new CSeq_feat());
                feat->SetLocation().SetInt().SetId().Assign(*entry.SetSeq().SetId().front());
                feat->SetLocation().SetInt().SetFrom(it_loc->first + 1);
                feat->SetLocation().SetInt().SetTo(it_loc->first + it_loc->second);
                feat->SetData().SetImp().SetKey("misc_feature");
                feat->SetComment("possible contamination");

                CRef<CSeq_annot> set_annot(new CSeq_annot);
                set_annot->SetData().SetFtable().push_back(feat);
                entry.SetSeq().SetAnnot().push_back(set_annot);
            }
        }
        ++it;
    }

    return false;
}

void CForeignContaminationScreenReportReader::PostProcess(CSeq_entry& entry)
{
    if (entry.IsSet())
    {
        CSeq_entry::TSet::TSeq_set::iterator it = entry.SetSet().SetSeq_set().begin();
        while (it != entry.SetSet().SetSeq_set().end())
        {
            if ((**it).IsSeq())
            {
                if (AnnotateOrRemove(**it))
                {
                    entry.SetSet().SetSeq_set().erase(it++);
                    continue;
                }

            }
            ++it;
        }
    }
}

END_NCBI_SCOPE
