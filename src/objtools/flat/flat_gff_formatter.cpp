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
* Author:  Aaron Ucko, Wratko Hlavina
*
* File Description:
*   Flat formatter for Generic Feature Format (incl. Gene Transfer Format)
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <objtools/flat/flat_gff_formatter.hpp>
#include <objtools/flat/flat_head.hpp>
#include <objtools/flat/flat_items.hpp>

#include <objects/general/Dbtag.hpp>
#include <objects/general/Int_fuzz.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>

#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CFlatGFFFormatter::CFlatGFFFormatter(IFlatTextOStream& stream, CScope& scope,
                                     EMode mode, TGFFFlags gff_flags,
                                     EStyle style, TFlags flags)
    : IFlatFormatter(scope, mode, style, flags),
      m_GFFFlags(gff_flags), m_Stream(&stream)
{
    list<string> header;
    header.push_back("##gff-version 2");
    header.push_back("##source-version NCBI C++ formatter 0.1");
    header.push_back("##date " + CurrentTime().AsString("Y-M-D"));
    stream.AddParagraph(header);
}


void CFlatGFFFormatter::FormatHead(const CFlatHead& head)
{
    m_Stream->NewSequence();
    list<string> l;

    switch (m_Context->GetMol()) {
    case CSeq_inst::eMol_dna:  m_SeqType = "DNA";      break;
    case CSeq_inst::eMol_rna:  m_SeqType = "RNA";      break;
    case CSeq_inst::eMol_aa:   m_SeqType = "Protein";  break;
    default:                   m_SeqType.erase();      break;
    }
    if ( !m_SeqType.empty() ) {
        l.push_back("##Type " + m_SeqType + ' '
                    + m_Context->GetAccession());
    }

    m_Date.erase();
    head.GetUpdateDate().GetDate(&m_Date, "%4Y-%{%2M%|??%}-%{%2D%|??%}");
    m_Strandedness = head.GetStrandedness();

    m_EndSequence.erase();
    m_Stream->AddParagraph(l, &head);
}


void CFlatGFFFormatter::FormatFeature(const IFlattishFeature& f)
{
    const CSeq_feat& seqfeat = f.GetFeat();
    string           key(f.GetKey()), oldkey;
    bool             gtf     = false;
    // CSeq_loc         tentative_stop;

    if ((m_GFFFlags & fGTFCompat)  &&  !m_Context->IsProt()
        &&  (key == "CDS"  ||  key == "exon")) {
        gtf = true;
    } else if ((m_GFFFlags & fGTFCompat)
               &&  m_Context->GetMol() == CSeq_inst::eMol_dna
               &&  seqfeat.GetData().IsRna()) {
        oldkey = key;
        key    = "exon";
        gtf    = true;
    } else if ((m_GFFFlags & fGTFOnly) == fGTFOnly) {
        return;
    }

    CFlatFeature& feat = *f.Format();
    list<string>  l;
    list<string>  attr_list;

    if ( !oldkey.empty() ) {
        attr_list.push_back("gbkey \"" + oldkey + "\";");
    }

    ITERATE (CFlatFeature::TQuals, it, feat.GetQuals()) {
        string name = (*it)->GetName();
        if (name == "codon_start"  ||  name == "translation"
            ||  name == "transcription") {
            continue; // suppressed to reduce verbosity
        } else if (name == "number"  &&  key == "exon") {
            name = "exon_number";
        } else if ((m_GFFFlags & fGTFCompat)  &&  !m_Context->IsProt()
                   &&  name == "gene") {
            string gene_id = x_GetGeneID(feat, (*it)->GetValue());
            attr_list.push_front
                ("transcript_id \"" + gene_id + '.' + m_Date + "\";");
            attr_list.push_front("gene_id \"" + gene_id + "\";");
            continue;
        }
        string value;
        NStr::Replace((*it)->GetValue(), " \b", kEmptyStr, value);
        string value2(NStr::PrintableString(value));
        // some parsers may be dumb, so quote further
        value.erase();
        ITERATE (string, c, value2) {
            switch (*c) {
            case ' ':  value += "\\x20"; break;
            case '\"': value += "x22";   break; // already backslashed
            case '#':  value += "\\x23"; break;
            default:   value += *c;
            }
        }
        attr_list.push_back(name + " \"" + value + "\";");
    }
    string attrs(NStr::Join(attr_list, " "));

    string source = x_GetSourceName(f);

    int frame = -1;
    if (seqfeat.GetData().IsCdregion()  &&  !m_Context->IsProt() ) {
        const CCdregion& cds = seqfeat.GetData().GetCdregion();
        frame = max(cds.GetFrame() - 1, 0);
    }
    x_AddFeature(l, f.GetLoc(), source, key, "." /*score*/, frame, attrs, gtf);

    if (gtf  &&  seqfeat.GetData().IsCdregion()) {
        const CCdregion& cds = seqfeat.GetData().GetCdregion();
        if ( !f.GetLoc().IsPartialStart(eExtreme_Biological) ) {
            CRef<CSeq_loc> tentative_start;
            {{
                CRef<SRelLoc::TRange> range(new SRelLoc::TRange);
                SRelLoc::TRanges      ranges;
                range->SetFrom(frame);
                range->SetTo(frame + 2);
                ranges.push_back(range);
                tentative_start = SRelLoc(f.GetLoc(), ranges).Resolve(m_Scope);
            }}

            string s;
            CSeqVector vect(*tentative_start,
                            m_Context->GetHandle().GetScope());
            vect.GetSeqData(0, 3, s);
            const CTrans_table* tt;
            if (cds.IsSetCode()) {
                tt = &CGen_code_table::GetTransTable(cds.GetCode());
            } else {
                tt = &CGen_code_table::GetTransTable(1);
            }
            if (s.size() == 3
                &&  tt->IsAnyStart(tt->SetCodonState(s[0], s[1], s[2]))) {
                x_AddFeature(l, *tentative_start, source, "start_codon",
                             "." /* score */, 0, attrs, gtf);
            }
        }

        if ( !f.GetLoc().IsPartialStop(eExtreme_Biological)  &&  seqfeat.IsSetProduct() ) {
            TSeqPos loc_len = sequence::GetLength(f.GetLoc(), m_Scope);
            TSeqPos prod_len = sequence::GetLength(seqfeat.GetProduct(),
                                                   m_Scope);
            CRef<CSeq_loc> tentative_stop;
            if (loc_len >= frame + 3 * prod_len + 3) {
                SRelLoc::TRange range;
                range.SetFrom(frame + 3 * prod_len);
                range.SetTo  (frame + 3 * prod_len + 2);
                // needs to be partial for TranslateCdregion to DTRT
                range.SetFuzz_from().SetLim(CInt_fuzz::eLim_lt);
                SRelLoc::TRanges ranges;
                ranges.push_back(CRef<SRelLoc::TRange>(&range));
                tentative_stop = SRelLoc(f.GetLoc(), ranges).Resolve(m_Scope);
            }
            if (tentative_stop.NotEmpty()  &&  !tentative_stop->IsNull()) {
                string s;
                CCdregion_translate::TranslateCdregion
                    (s, m_Context->GetHandle(), *tentative_stop, cds);
                if (s == "*") {
                    x_AddFeature(l, *tentative_stop, source, "stop_codon",
                                 "." /* score */, 0, attrs, gtf);
                }
            }
        }
    }

    m_Stream->AddParagraph(l, &f, &seqfeat);
}


void CFlatGFFFormatter::FormatDataHeader(const CFlatDataHeader& dh)
{
    if ( !(m_GFFFlags & fShowSeq) )
        return;

    list<string> l;
    l.push_back("##" + m_SeqType + ' ' + m_Context->GetAccession());
    m_Stream->AddParagraph(l, &dh);
    m_EndSequence = "##end-" + m_SeqType;
}


void CFlatGFFFormatter::FormatData(const CFlatData& data)
{
    if ( !(m_GFFFlags & fShowSeq) )
        return;

    list<string> l;
    CSeqVector v(data.GetLoc(), m_Context->GetHandle().GetScope(),
                 CBioseq_Handle::eCoding_Iupac);
    CSeqVector_CI vi(v);
    while (vi) {
        string s;
        vi.GetSeqData(s, 70);
        l.push_back("##" + s);
    }
    m_Stream->AddParagraph(l, &data, &data.GetLoc());
}


void CFlatGFFFormatter::EndSequence(void)
{
    if ( !m_EndSequence.empty() ) {
        list<string> l;
        l.push_back(m_EndSequence);
        m_Stream->AddParagraph(l);
    }
}


string CFlatGFFFormatter::x_GetGeneID(const CFlatFeature& feat,
                                      const string& gene)
{
    const CSeq_feat& seqfeat = feat.GetFeat();

    string               main_acc;
    if (m_Context->InSegSet()) {
        const CSeq_id& id = *m_Context->GetSegMaster()->GetId().front();
        main_acc = m_Context->GetPreferredSynonym(id).GetSeqIdString(true);
    } else {
        main_acc = m_Context->GetAccession();
    }

    string               gene_id   = main_acc + ':' + gene;
    CConstRef<CSeq_feat> gene_feat = sequence::GetBestOverlappingFeat
        (seqfeat.GetLocation(), CSeqFeatData::e_Gene,
         sequence::eOverlap_Interval, *m_Scope);
    
    TFeatVec&                v  = m_Genes[gene_id];
    TFeatVec::const_iterator it = find(v.begin(), v.end(), gene_feat);
    int                      n;
    if (it == v.end()) {
        n = v.size();
        v.push_back(gene_feat);
    } else {
        n = it - v.begin();
    }
    if (n > 0) {
        gene_id += '.' + NStr::IntToString(n + 1);
    }

    return gene_id;
}


string CFlatGFFFormatter::x_GetSourceName(const IFlattishFeature&)
{
    // XXX - get from annot name (not presently available from IFF)?
    switch (m_Context->GetPrimaryID().Which()) {
    case CSeq_id::e_Local:                           return "Local";
    case CSeq_id::e_Gibbsq: case CSeq_id::e_Gibbmt:
    case CSeq_id::e_Giim:   case CSeq_id::e_Gi:      return "GenInfo";
    case CSeq_id::e_Genbank:                         return "Genbank";
    case CSeq_id::e_Swissprot:                       return "SwissProt";
    case CSeq_id::e_Patent:                          return "Patent";
    case CSeq_id::e_Other:                           return "RefSeq";
    case CSeq_id::e_General:
        return m_Context->GetPrimaryID().GetGeneral().GetDb();
    default:
    {
        string source
            (CSeq_id::SelectionName(m_Context->GetPrimaryID().Which()));
        return NStr::ToUpper(source);
    }
    }
}


void CFlatGFFFormatter::x_AddFeature(list<string>& l, const CSeq_loc& loc,
                                     const string& source, const string& key,
                                     const string& score, int frame,
                                     const string& attrs, bool gtf)
{
    int exon_number = 1;
    for (CSeq_loc_CI it(loc);  it;  ++it) {
        TSeqPos from   = it.GetRange().GetFrom(), to = it.GetRange().GetTo();
        char    strand = '+';

        if (IsReverse(it.GetStrand())) {
            strand = '-';
        } else if (it.GetRange().IsWhole()
                   ||  (m_Strandedness <= CSeq_inst::eStrand_ss
                        &&  m_Context->GetMol() != CSeq_inst::eMol_dna)) {
            strand = '.'; // N/A
        }

        if (it.GetRange().IsWhole()) {
            to = sequence::GetLength(it.GetSeq_id(), m_Scope) - 1;
        }

        string extra_attrs;
        if (gtf  &&  attrs.find("exon_number ") == NPOS) {
            CSeq_loc       loc2;
            CSeq_interval& si = loc2.SetInt();
            si.SetFrom(from);
            si.SetTo(to);
            si.SetStrand(it.GetStrand());
            si.SetId(const_cast<CSeq_id&>(it.GetSeq_id()));
            CConstRef<CSeq_feat> exon = sequence::GetBestOverlappingFeat
                (loc2, CSeqFeatData::eSubtype_exon,
                 sequence::eOverlap_Contains, *m_Scope);
            if (exon.NotEmpty()  &&  exon->IsSetQual()) {
                ITERATE (CSeq_feat::TQual, q, exon->GetQual()) {
                    if ( !NStr::CompareNocase((*q)->GetQual(), "number") ) {
                        int n = NStr::StringToNumeric((*q)->GetVal());
                        if (n >= exon_number) {
                            exon_number = n;
                            break;
                        }
                    }
                }
            }
            extra_attrs = " exon_number \"" + NStr::IntToString(exon_number)
                + "\";";
            ++exon_number;
        }

        if ( sequence::IsSameBioseq(it.GetSeq_id(), m_Context->GetPrimaryID(),
                                    m_Scope) ) {
            // conditionalize printing, but update state regardless
            l.push_back(m_Context->GetAccession() + '\t'
                        + source + '\t'
                        + key + '\t'
                        + NStr::UIntToString(from + 1) + '\t'
                        + NStr::UIntToString(to + 1) + '\t'
                        + score + '\t'
                        + strand + '\t'
                        + (frame >= 0 ? char(frame + '0') : '.') + "\t"
                        + attrs + extra_attrs);
        }

        if (frame >= 0) {
            frame = (frame + to - from + 1) % 3;
        }
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.8  2005/02/18 15:05:12  shomrat
* CSeq_loc interface changes
*
* Revision 1.7  2004/12/06 17:54:10  grichenk
* Replaced calls to deprecated methods
*
* Revision 1.6  2004/05/21 21:42:53  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.5  2003/12/03 20:53:53  ucko
* Also quote #s in values to avoid trouble with naive comment recognizers.
*
* Revision 1.4  2003/11/04 20:00:28  ucko
* Edit " \b" sequences (used as hints for wrapping) out from qualifier values
*
* Revision 1.3  2003/10/18 01:36:34  ucko
* Tweak to work around MSVC stupidity.
*
* Revision 1.2  2003/10/17 21:06:35  ucko
* Reworked GTF mode per Wratko's critique:
*  - Now handles multi-exonic(!) start and stop codons.
*  - Treats all RNA features on DNA as exons.
*  - Sets exon_number attribute for GTF 1 compatibility.
*  - Quotes other attributes better.
*
* Revision 1.1  2003/10/08 21:11:45  ucko
* New GFF/GTF formatter
*
*
* ===========================================================================
*/
