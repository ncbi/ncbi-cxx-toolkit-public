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
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko, Mati Shomrat, ....
 *
 * File Description:
 *   Implementation of private parts of the validator
 *   .......
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>
#include <objmgr/object_manager.hpp>

#include <corelib/ncbiexec.hpp>
#include <objtools/validator/validator_barcode.hpp>
#include <objtools/validator/utilities.hpp>

#include <serial/iterator.hpp>
#include <serial/enumvalues.hpp>

#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <objects/seq/Bioseq.hpp>


#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/seq_annot_ci.hpp>
#include <objmgr/util/sequence.hpp>

#include <objmgr/feat_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/scope.hpp>

#include <objects/misc/sequence_macros.hpp>


#include <objtools/error_codes.hpp>
#include <objtools/edit/seq_entry_edit.hpp>
#include <util/sgml_entity.hpp>
#include <util/line_reader.hpp>
#include <util/util_misc.hpp>
#include <util/static_set.hpp>

#include <algorithm>


#include <serial/iterator.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)
//using namespace sequence;



string GetSeqTitle(CBioseq_Handle bsh)
{
    string accession;
    string local;
    string label;
    for (CBioseq_Handle::TId::const_iterator it = bsh.GetId().begin(); it != bsh.GetId().end(); ++it)
    {
        const CSeq_id &id = *(it->GetSeqId());
        if (id.IsGenbank() && id.GetGenbank().IsSetAccession())
            accession = id.GetGenbank().GetAccession();
        if (id.IsLocal() && id.GetLocal().IsStr())
            local = id.GetLocal().GetStr();
    }
    if (!accession.empty())
        return accession;
    if (!local.empty())
        return local;

    bsh.GetBioseqCore()->GetLabel(&label, CBioseq::eContent);
    return label;
}

string GetBarcodeId(CBioseq_Handle bsh)
{
    string barcode;
    string local;
    for (CBioseq_Handle::TId::const_iterator it = bsh.GetId().begin(); it != bsh.GetId().end(); ++it)
    {
        const CSeq_id &id = *(it->GetSeqId());
        if (id.IsGeneral() && id.GetGeneral().IsSetDb() && NStr::EqualNocase(id.GetGeneral().GetDb(), "uoguelph"))
        {
            id.GetLabel(&barcode, CSeq_id::eContent);
            NStr::ReplaceInPlace(barcode, "uoguelph:", kEmptyStr);
        }
        if (id.IsLocal())
        {
            id.GetLabel(&local, CSeq_id::eContent);
        }
    }
    if (!barcode.empty())
        return barcode;
    if (!local.empty())
        return local;

    return "NO";
}


bool GetIsLength(CBioseq_Handle bsh)
{
    bool found_rbcl(false);
    bool found_matk(false);

    for (CFeat_CI feat_it(bsh, CSeqFeatData::eSubtype_gene); feat_it; ++feat_it)
    {
        const CSeq_feat& gene = feat_it->GetOriginalFeature();
        if (gene.IsSetData() && gene.GetData().IsGene() && gene.GetData().GetGene().IsSetLocus())
        {
            if (NStr::EqualNocase(gene.GetData().GetGene().GetLocus(), "rbcL"))
                found_rbcl = true;
            if (NStr::EqualNocase(gene.GetData().GetGene().GetLocus(), "matK"))
                found_matk = true;
        }
    }
    TSeqPos length = bsh.GetBioseqLength();
    bool rval(false);

    if (found_matk)
    {
        if (length < 585)
        {
            rval = true;
        }
    } else if (found_rbcl)
    {
        if (length < 414)
        {
            rval = true;
        }
    } else if (length < 500)
    {
        rval = true;
    }
    return rval;
}

bool GetIsPrimers(CBioseq_Handle bsh)
{
    bool forward(false);
    bool reverse(false);
    for (CSeqdesc_CI source_ci(bsh, CSeqdesc::e_Source); source_ci; ++source_ci)
    {
        if (source_ci->GetSource().IsSetPcr_primers())
        {
            FOR_EACH_PCRREACTION_IN_PCRREACTIONSET(reaction, source_ci->GetSource().GetPcr_primers())
            {
                if ((*reaction)->IsSetForward() && (*reaction)->GetForward().IsSet() && !(*reaction)->GetForward().Get().empty())
                    forward = true;
                if ((*reaction)->IsSetReverse() && (*reaction)->GetReverse().IsSet() && !(*reaction)->GetReverse().Get().empty())
                    reverse = true;
            }
        }
    }
    return !(forward && reverse);
}

bool GetIsCountry(CBioseq_Handle bsh)
{
    bool country(false);
    for (CSeqdesc_CI source_ci(bsh, CSeqdesc::e_Source); source_ci; ++source_ci)
    {
        FOR_EACH_SUBSOURCE_ON_BIOSOURCE(subsource, source_ci->GetSource())
        {
            if ((*subsource)->IsSetSubtype() && (*subsource)->GetSubtype() == CSubSource::eSubtype_country)
            {
                country = true;
            }
        }
    }
    return !country;
}

bool GetIsVoucher(CBioseq_Handle bsh)
{
    bool rval(false);
    for (CSeqdesc_CI source_ci(bsh, CSeqdesc::e_Source); source_ci; ++source_ci)
    {
        FOR_EACH_ORGMOD_ON_BIOSOURCE(orgmod, source_ci->GetSource())
        {
            if ((*orgmod)->IsSetSubtype() && (
                (*orgmod)->GetSubtype() == COrgMod::eSubtype_specimen_voucher ||
                (*orgmod)->GetSubtype() == COrgMod::eSubtype_bio_material ||
                (*orgmod)->GetSubtype() == COrgMod::eSubtype_culture_collection))
            {
                rval = true;
            }
        }
    }
    return !rval;
}

bool GetIsStructuredVoucher(CBioseq_Handle bsh)
{
    bool rval(false);
    for (CSeqdesc_CI source_ci(bsh, CSeqdesc::e_Source); source_ci; ++source_ci)
    {
        FOR_EACH_ORGMOD_ON_BIOSOURCE(orgmod, source_ci->GetSource())
        {
            if ((*orgmod)->IsSetSubtype() && (
                (*orgmod)->GetSubtype() == COrgMod::eSubtype_specimen_voucher ||
                (*orgmod)->GetSubtype() == COrgMod::eSubtype_bio_material ||
                (*orgmod)->GetSubtype() == COrgMod::eSubtype_culture_collection)
                && (*orgmod)->IsSetSubname())
            {
                string subname = (*orgmod)->GetSubname();
                if (NStr::Find(subname, ":") != NPOS)
                    rval = true;
            }
        }
    }
    return rval;
}

string GetPercentN(CBioseq_Handle bsh)
{
    TSeqPos num_n = 0;
    CBioseqGaps_CI::Params  params;
    params.max_gap_len_to_ignore = 0;
    for (CBioseqGaps_CI gap_it(bsh.GetSeq_entry_Handle(), params); gap_it; ++gap_it)
    {
        num_n += gap_it->length;
    }
    double p = double(100 * num_n) / bsh.GetBioseqLength();
    string percent;
    if (p > 1.)
        percent = NStr::DoubleToString(p, 1);
    return percent;
}

bool GetHasCollectionDate(CBioseq_Handle bsh)
{
    bool has_date(false);
    for (CSeqdesc_CI source_ci(bsh, CSeqdesc::e_Source); source_ci; ++source_ci)
    {
        FOR_EACH_SUBSOURCE_ON_BIOSOURCE(subsource, source_ci->GetSource())
        {
            if ((*subsource)->IsSetSubtype() && (*subsource)->GetSubtype() == CSubSource::eSubtype_collection_date && (*subsource)->IsSetName())
            {
                const string &date = (*subsource)->GetName();
                bool bad_format(false);
                bool in_future(false);
                CSubSource::IsCorrectDateFormat(date, bad_format, in_future);
                if (!bad_format && !in_future)
                {
                    has_date = true;
                }
            }
        }
    }
    return !has_date;
}

bool GetHasOrderAssignment(CBioseq_Handle bsh)
{
    bool has_order(false);
    bool has_ibol(false);
    for (CSeqdesc_CI desc_ci(bsh, CSeqdesc::e_User); desc_ci; ++desc_ci)
    {
        if (desc_ci->GetUser().IsSetType() && desc_ci->GetUser().GetType().IsStr() && NStr::EqualNocase(desc_ci->GetUser().GetType().GetStr(), "StructuredComment"))
        {
            bool is_ibol(false);
            if (desc_ci->GetUser().HasField("StructuredCommentPrefix"))
            {
                const CUser_field& field = desc_ci->GetUser().GetField("StructuredCommentPrefix");
                if (field.IsSetData() && field.GetData().IsStr() && NStr::EqualNocase(field.GetData().GetStr(), "##International Barcode of Life (iBOL)Data-START##"))
                {
                    is_ibol = true;
                    has_ibol = true;
                }
            }
            if (is_ibol && desc_ci->GetUser().HasField("Order Assignment"))
            {
                const CUser_field& field = desc_ci->GetUser().GetField("Order Assignment");
                if (field.IsSetData() && field.GetData().IsStr() && !field.GetData().GetStr().empty())
                {
                    has_order = true;
                }
            }
        }
    }
    return has_ibol && !has_order;
}

bool GetLowTrace(CBioseq_Handle bsh, bool &found)
{
    bool low_trace(false);
    found = false;
    for (CSeqdesc_CI desc_ci(bsh, CSeqdesc::e_User); desc_ci; ++desc_ci)
    {
        if (desc_ci->GetUser().IsSetType() && desc_ci->GetUser().GetType().IsStr() && NStr::EqualNocase(desc_ci->GetUser().GetType().GetStr(), "Submission"))
        {
            if (desc_ci->GetUser().HasField("AdditionalComment"))
            {
                const CUser_field& field = desc_ci->GetUser().GetField("AdditionalComment");
                if (field.IsSetData() && field.GetData().IsStr() && NStr::StartsWith(field.GetData().GetStr(), "Traces: "))
                {
                    string str = field.GetData().GetStr();
                    NStr::ReplaceInPlace(str, "Traces: ", kEmptyStr);
                    int traces = NStr::StringToInt(str, NStr::fConvErr_NoThrow | NStr::fAllowLeadingSpaces | NStr::fAllowTrailingSpaces);
                    found = true;
                    if (traces < 2)
                        low_trace = true;
                }
            }
        }
    }
    return low_trace;
}


bool BarcodeTestFails(const SBarcode& b)
{
    if (b.length || b.primers || b.country || b.voucher || !b.structured_voucher || // NO b.structured_voucher or b.has_keyword
        !b.percent_n.empty() || b.collection_date || b.order_assignment || b.low_trace || b.frame_shift)
    {
        return true;
    } else {
        return false;
    }
}


void RunLowTraceScript(map<string, pair<SBarcode, int> >& trace_lookup, TBarcodeResults& failures)
{
    map<string, int> m_cache_lookup;

    {
        map<string, pair<SBarcode, int> >::iterator it = trace_lookup.begin();
        while (it != trace_lookup.end())
        {
            if (m_cache_lookup.find(it->first) != m_cache_lookup.end())
            {
                SBarcode b = it->second.first;
                if (m_cache_lookup[it->first] < 2)
                {
                    b.low_trace = true;
                }

                if (BarcodeTestFails(b))
                {
                    failures.push_back(b);
                }
                it = trace_lookup.erase(it);
            } else
                ++it;
        }
    }

    if (trace_lookup.empty())
        return;

#ifdef NCBI_OS_MSWIN
    string script = "\\\\snowman\\win-coremake\\App\\Ncbi\\smart\\bin\\cmd_sequin_fetch_from_Trace.bat";
#else
    string script = "/netopt/genbank/subtool/bin/cmd_sequin_fetch_from_Trace";
#endif

    if (!CFile(script).Exists())
    {
        NCBI_THROW(CCoreException, eCore, "Path to Fetch from Trace script does not exist");
    } else {
        string tmpIn = CFile::GetTmpName();
        {
            CNcbiOfstream ostr(tmpIn.c_str());
            for (map<string, pair<SBarcode, int> >::iterator it = trace_lookup.begin(); it != trace_lookup.end(); ++it)
            {
                ostr << it->first << endl;
            }
        }

        string tmpOut = CFile::GetTmpName();
        string cmdline = script + " -i " + tmpIn + " -o " + tmpOut;
        CExec::System(cmdline.c_str());
        if (!tmpIn.empty()) {
            CFile(tmpIn).Remove();
        }
        CNcbiIfstream istr(tmpOut.c_str());
        string str;
        while (NcbiGetline(istr, str, "\r\n")) {
            if (NStr::StartsWith(str, "ERROR"))
                break;
            string acc, ti;
            NStr::SplitInTwo(str, "\t", acc, ti);
            trace_lookup[acc].second++;
        }
        CFile(tmpOut).Remove();
    }

    for (map<string, pair<SBarcode, int> >::iterator it = trace_lookup.begin(); it != trace_lookup.end(); ++it)
    {
        m_cache_lookup[it->first] = it->second.second;
        SBarcode b = it->second.first;
        if (it->second.second < 2)
        {
            b.low_trace = true;
        }

        if (BarcodeTestFails(b))
        {
            failures.push_back(b);
        }
    }
}

bool GetHasFrameShift(CBioseq_Handle bsh)
{
    bool is_ibol(false);
    for (CSeqdesc_CI desc_ci(bsh, CSeqdesc::e_User); desc_ci; ++desc_ci)
    {
        if (desc_ci->GetUser().IsSetType() && desc_ci->GetUser().GetType().IsStr() && NStr::EqualNocase(desc_ci->GetUser().GetType().GetStr(), "StructuredComment"))
        {
            if (desc_ci->GetUser().HasField("StructuredCommentPrefix"))
            {
                const CUser_field& field = desc_ci->GetUser().GetField("StructuredCommentPrefix");
                if (field.IsSetData() && field.GetData().IsStr() && NStr::EqualNocase(field.GetData().GetStr(), "##International Barcode of Life (iBOL)Data-START##"))
                {
                    is_ibol = true;
                }
            }
        }
    }

    bool frame_shift(false);
    for (CSeqdesc_CI desc_ci(bsh, CSeqdesc::e_User); desc_ci; ++desc_ci)
    {
        if (desc_ci->GetUser().IsSetType() && desc_ci->GetUser().GetType().IsStr() && NStr::EqualNocase(desc_ci->GetUser().GetType().GetStr(), "multalin"))
        {
            if (desc_ci->GetUser().HasField("frameshift-nuc"))
            {
                const CUser_field& field = desc_ci->GetUser().GetField("frameshift-nuc");
                if (field.IsSetData() && field.GetData().IsStr() && NStr::EqualNocase(field.GetData().GetStr(), "fail"))
                {
                    frame_shift = true;
                }
            }
        }
    }

    return is_ibol && frame_shift;
}

bool GetHasKeyword(CBioseq_Handle bsh)
{
    bool has_keyword(false);
    for (CSeqdesc_CI desc_ci(bsh, CSeqdesc::e_Genbank); desc_ci; ++desc_ci)
    {
        FOR_EACH_KEYWORD_ON_GENBANKBLOCK(qual_it, desc_ci->GetGenbank())
        {
            const string &keyword = *qual_it;
            if (NStr::EqualNocase(keyword, "BARCODE"))
                has_keyword = true;
        }
    }
    return has_keyword;
}

bool IsTechBarcode(CBioseq_Handle bsh)
{
    bool res(false);
    for (CSeqdesc_CI desc_it(bsh, CSeqdesc::e_Molinfo); desc_it; ++desc_it)
    {
        if (desc_it->GetMolinfo().IsSetTech() && desc_it->GetMolinfo().GetTech() == CMolInfo::eTech_barcode)
        {
            res = true;
        }
    }
    return res;
}


void BarcodeTestBioseq(CBioseq_Handle bsh, SBarcode& b, bool &trace_present)
{
    if (!IsTechBarcode(bsh))
        return;

    b.bsh = bsh;
    b.barcode = GetBarcodeId(bsh);
    b.genbank = GetSeqTitle(bsh);
    b.length = GetIsLength(bsh);
    b.primers = GetIsPrimers(bsh);
    b.country = GetIsCountry(bsh);
    b.voucher = GetIsVoucher(bsh);
    b.structured_voucher = GetIsStructuredVoucher(bsh);
    b.percent_n = GetPercentN(bsh);
    b.collection_date = GetHasCollectionDate(bsh);
    b.order_assignment = GetHasOrderAssignment(bsh);
    b.low_trace = GetLowTrace(bsh, trace_present);
    b.frame_shift = GetHasFrameShift(bsh);
    b.has_keyword = GetHasKeyword(bsh);
}


TBarcodeResults GetBarcodeValues(CSeq_entry_Handle seh)
{
    TBarcodeResults BarcodeFailures;

    map<string, pair<SBarcode, int> > trace_lookup;

    objects::CBioseq_CI b_iter(seh, objects::CSeq_inst::eMol_na);
    for (; b_iter; ++b_iter)
    {
        SBarcode b;
        bool trace_present(false);
        BarcodeTestBioseq(*b_iter, b, trace_present);
        if (!trace_present)
        {
            trace_lookup[b.genbank] = pair<SBarcode, int>(b, 0);
        } else if (BarcodeTestFails(b)) {
            BarcodeFailures.push_back(b);
        }
    }
    RunLowTraceScript(trace_lookup, BarcodeFailures);

    return BarcodeFailures;
}


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE
