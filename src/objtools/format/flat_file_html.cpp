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
* Author:  Jonathan Kans
*
* File Description:
*   Configuration class for flat-file HTML generator
*
*/
#include <ncbi_pch.hpp>
#include <objtools/format/flat_file_config.hpp>
#include <objtools/format/flat_file_html.hpp>
#include <util/static_map.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiargs.hpp>

#include <objtools/format/items/accession_item.hpp>
#include <objtools/format/items/basecount_item.hpp>
#include <objtools/format/items/comment_item.hpp>
#include <objtools/format/items/contig_item.hpp>
#include <objtools/format/items/ctrl_items.hpp>
#include <objtools/format/items/dbsource_item.hpp>
#include <objtools/format/items/defline_item.hpp>
#include <objtools/format/items/feature_item.hpp>
#include <objtools/format/items/gap_item.hpp>
#include <objtools/format/items/genome_project_item.hpp>
#include <objtools/format/items/html_anchor_item.hpp>
#include <objtools/format/items/keywords_item.hpp>
#include <objtools/format/items/locus_item.hpp>
#include <objtools/format/items/origin_item.hpp>
#include <objtools/format/items/primary_item.hpp>
#include <objtools/format/items/reference_item.hpp>
#include <objtools/format/items/segment_item.hpp>
#include <objtools/format/items/sequence_item.hpp>
#include <objtools/format/items/source_item.hpp>
#include <objtools/format/items/tsa_item.hpp>
#include <objtools/format/items/version_item.hpp>
#include <objtools/format/items/wgs_item.hpp>
#include <objtools/format/flat_expt.hpp>

#include <objmgr/util/objutil.hpp>
#include <objmgr/scope.hpp>

#include <objects/seqloc/Seq_bond.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CHTMLFormatterEx::CHTMLFormatterEx(CRef<CScope> scope) :
    m_scope(scope)
{
}

CHTMLFormatterEx::~CHTMLFormatterEx()
{
}

void CHTMLFormatterEx::FormatProteinId(string& str, const CSeq_id& seq_id, const string& prot_id) const
{
    string                 index = prot_id;
    CBioseq_Handle         bsh   = m_scope->GetBioseqHandle(seq_id);
    vector<CSeq_id_Handle> ids   = bsh.GetId();
    for (auto hid: ids) {
        if (hid.IsGi()) {
            index = NStr::NumericToString(hid.GetGi());
            break;
        }
    }
    str = "<a href=\"";
    str += strLinkBaseProt;
    str += index;
    str += "\">";
    str += prot_id;
    str += "</a>";
}

void CHTMLFormatterEx::FormatTranscriptId(string& str, const CSeq_id& seq_id, const string& nuc_id) const
{
    string                 index = nuc_id;
    CBioseq_Handle         bsh   = m_scope->GetBioseqHandle(seq_id);
    vector<CSeq_id_Handle> ids   = bsh.GetId();
    for(auto hid: ids) {
        if (hid.IsGi()) {
            index = NStr::NumericToString(hid.GetGi());
            break;
        }
    }
    str = "<a href=\"";
    str += strLinkBaseNuc;
    str += index;
    str += "\">";
    str += nuc_id;
    str += "</a>";
}

void CHTMLFormatterEx::FormatNucId(string& str, const CSeq_id&, TIntId gi, const string& acc_id) const
{
    str = "<a href=\"";
    str += strLinkBaseNuc + NStr::NumericToString(gi) + "\">" + acc_id + "</a>";
}

void CHTMLFormatterEx::FormatTaxid(string& str, const TTaxId taxid, const string& taxname) const
{
    if (! NStr::StartsWith(taxname, "Unknown", NStr::eNocase)) {
        if (taxid > ZERO_TAX_ID) {
            str += "<a href=\"";
            str += strLinkBaseTaxonomy;
            str += "id=";
            str += NStr::NumericToString(taxid);
            str += "\">";
        } else {
            string t_taxname = taxname;
            replace(t_taxname.begin(), t_taxname.end(), ' ', '+');
            str += "<a href=\"";
            str += strLinkBaseTaxonomy;
            str += "name=";
            str += taxname;
            str += "\">";
        }
        str += taxname;
        str += "</a>";
    } else {
        str = taxname;
    }

    TryToSanitizeHtml(str);
}

void CHTMLFormatterEx::FormatNucSearch(CNcbiOstream& os, const string& id) const
{
    os << "<a href=\"" << strLinkBaseNucSearch << id << "\">" << id << "</a>";
}

static void s_AddSeqIntString(const CSeq_interval& seqint, string& loc_str,
                              bool& add_comma)
{
    TSeqPos iFrom = seqint.GetFrom() + 1;
    TSeqPos iTo = seqint.GetTo() + 1;

    if (add_comma) loc_str += ",";

    loc_str += NStr::IntToString(iFrom) + ":" + NStr::IntToString(iTo);

    // When location is on the reverse strand, add the strand value
    if (seqint.CanGetStrand() && seqint.GetStrand() == eNa_strand_minus)
        loc_str += ":2";

    add_comma = true;
}

static void
s_AddSeqPointString(const CSeq_point& seqpnt, string& loc_str, bool& add_comma)
{
    string pnt_str = NStr::IntToString(seqpnt.GetPoint() + 1);

    if (add_comma) loc_str += ",";
    loc_str += pnt_str;

    // When location is on the reverse strand, add end-point (equal to start)
    // and strand value.
    if (seqpnt.CanGetStrand() && seqpnt.GetStrand() == eNa_strand_minus)
        loc_str += ":" + pnt_str + ":2";

    add_comma = true;
}

static void
s_AddSeqBondString(const CSeq_bond& seqbond, string& loc_str, bool& add_comma)
{
    // This actually should always be available, but just in case.
    if (seqbond.CanGetA()) {
        s_AddSeqPointString(seqbond.GetA(), loc_str, add_comma);
    }
    if (seqbond.CanGetB()) {
        s_AddSeqPointString(seqbond.GetB(), loc_str, add_comma);
    }
}

static void
s_AddSeqPackedPointString(const CPacked_seqpnt& packed_pnt, string& loc_str,
                          bool& add_comma)
{
    bool reverse =
        (packed_pnt.CanGetStrand() && packed_pnt.GetStrand() == eNa_strand_minus);
    string pnt_str;
            
    ITERATE(CPacked_seqpnt::TPoints, it, packed_pnt.GetPoints()) {
        if (add_comma) loc_str += ",";
        pnt_str = NStr::IntToString(*it + 1);
        loc_str += pnt_str;
        if (reverse)
            loc_str += ":" + pnt_str + ":2";
        add_comma = true;
    }
}

static void s_AddLocString(const CSeq_loc& loc, string& loc_str, bool& add_comma)
{
    switch(loc.Which()) {
    case CSeq_loc::e_Int:
        s_AddSeqIntString(loc.GetInt(), loc_str, add_comma);
        break;
    case CSeq_loc::e_Pnt:
        s_AddSeqPointString(loc.GetPnt(), loc_str, add_comma);
        break;
    case CSeq_loc::e_Bond:
        s_AddSeqBondString(loc.GetBond(), loc_str, add_comma);
        break;
    case CSeq_loc::e_Mix:
        ITERATE(CSeq_loc_mix::Tdata, loc_it, loc.GetMix().Get()) {
            s_AddLocString(**loc_it, loc_str, add_comma);
        }
        break;
    case CSeq_loc::e_Packed_int:
        ITERATE(CPacked_seqint::Tdata, it, loc.GetPacked_int().Get()) {
            s_AddSeqIntString(**it, loc_str, add_comma);
        }
        break;
    case CSeq_loc::e_Packed_pnt:
        s_AddSeqPackedPointString(loc.GetPacked_pnt(), loc_str, add_comma);
        break;
    default:
        break;
    }
}

void CHTMLFormatterEx::FormatLocation(string& strLink, const CSeq_loc& loc, TIntId gi, const string& visible_text) const
{
    string acc;
    // check if this is a protein or nucleotide link
    bool is_prot = false;
    {{
        CBioseq_Handle bioseq_h;
        for (auto& loc_ci: loc) {
            bioseq_h = m_scope->GetBioseqHandle(loc_ci.GetSeq_id());
            if (bioseq_h) {
                break;
            }
        }
        if (bioseq_h) {
            is_prot = (bioseq_h.GetBioseqMolType() == CSeq_inst::eMol_aa);
            CConstRef<CSeq_id> accid = FindBestChoice(bioseq_h.GetBioseqCore()->GetId(), CSeq_id::Score);
            acc = GetLabel(*accid);
        }
    }}

    // assembly of the actual string:
    strLink.reserve(100); // heuristical URL length

    strLink = "<a href=\"";

    // link base
    if (is_prot) {
        strLink += strLinkBaseProt;
    } else {
        strLink += strLinkBaseNuc;
    }
    // strLink += NStr::NumericToString(gi);
    strLink += acc;

    // location
    if (loc.IsInt() || loc.IsPnt()) {
        TSeqPos iFrom = loc.GetStart(eExtreme_Positional) + 1;
        TSeqPos iTo   = loc.GetStop(eExtreme_Positional) + 1;
        strLink += "?from=";
        strLink += NStr::IntToString(iFrom);
        strLink += "&amp;to=";
        strLink += NStr::IntToString(iTo);
    } else if (visible_text != "Precursor") {
        bool add_comma = false;
        strLink += "?location=";
        s_AddLocString(loc, strLink, add_comma);
    }

    strLink += "\">";
    strLink += visible_text;
    strLink += "</a>";
}

void CHTMLFormatterEx::FormatModelEvidence(string& str, const SModelEvidance& me) const
{
    str += "<a href=\"";
    str += strLinkBaseNuc;
    if (me.gi > ZERO_GI) {
        str += NStr::NumericToString(me.gi);
    } else {
        str += me.name;
    }
    str += "?report=graph";
    if ((me.span.first >= 0) && (me.span.second >= me.span.first)) {
        const Int8 kPadAmount = 500;
        // The "+1" is because we display 1-based to user and in URL
        str += "&v=";
        str += NStr::NumericToString(max<Int8>(me.span.first + 1 - kPadAmount, 1));
        str += ":";
        str += NStr::NumericToString(me.span.second + 1 + kPadAmount); // okay if second number goes over end of sequence
    }
    str += "\">";
    str += me.name;
    str += "</a>";
}

void CHTMLFormatterEx::FormatTranscript(string& str, const string& name) const
{
    str += "<a href=\"";
    str += strLinkBaseNuc;
    str += name;
    str += "\">";
    str += name;
    str += "</a>";
}

void CHTMLFormatterEx::FormatGeneralId(CNcbiOstream& os, const string& id) const
{
    os << "<a href=\"" << strLinkBaseNuc << id << "\">" << id << "</a>";
}

void CHTMLFormatterEx::FormatGapLink(CNcbiOstream& os, TSeqPos gap_size, const string& id, bool is_prot) const
{
    const string link_base = (is_prot ? strLinkBaseProt : strLinkBaseNuc);
    const char*  mol_type  = (is_prot ? "aa" : "bp");
    os << "          [gap " << gap_size << " " << mol_type << "]"
       << "    <a href=\"" << link_base << id << "?expand-gaps=on\">Expand Ns</a>";
}

void CHTMLFormatterEx::FormatUniProtId(string& str, const string& prot_id) const
{
    str = "<a href=\"";
    str += strLinkBaseUniProt;
    str += prot_id;
    str += "\">";
    str += prot_id;
    str += "</a>";
}

END_SCOPE(objects)
END_NCBI_SCOPE
