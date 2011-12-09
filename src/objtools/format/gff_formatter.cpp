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
* Author:  Aaron Ucko, NCBI
*          Mati Shomrat
*
* File Description:
*           
*
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objects/general/Date.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/seq_vector.hpp>
#include <objtools/format/gff_formatter.hpp>
#include <objtools/format/items/locus_item.hpp>
#include <objtools/format/items/date_item.hpp>
#include <objtools/format/items/feature_item.hpp>
#include <objtools/format/items/basecount_item.hpp>
#include <objtools/format/items/sequence_item.hpp>
#include <objtools/format/items/ctrl_items.hpp>
#include <objtools/format/context.hpp>
#include <objtools/error_codes.hpp>
#include <algorithm>


#define NCBI_USE_ERRCODE_X   Objtools_Fmt_GFF


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CGFFFormatter::CGFFFormatter(void)
{
}


void CGFFFormatter::Start(IFlatTextOStream& text_os)
{
    list<string> l;
    l.push_back("##gff-version 2");
    l.push_back("##source-version NCBI C++ formatter 0.3");
    text_os.AddParagraph(l);
}


void CGFFFormatter::StartSection(const CStartSectionItem& ssec, IFlatTextOStream& text_os)
{
    list<string> l;
    CBioseqContext& bctx = *ssec.GetContext();

    switch (bctx.GetMol()) {
    case CSeq_inst::eMol_dna:  m_SeqType = "DNA";      break;
    case CSeq_inst::eMol_rna:  m_SeqType = "RNA";      break;
    case CSeq_inst::eMol_aa:   m_SeqType = "Protein";  break;
    default:                   m_SeqType.erase();      break;
    }
    if ( !m_SeqType.empty() ) {
        l.push_back("##Type " + m_SeqType + ' '
                    + bctx.GetAccession());
    }
    text_os.AddParagraph(l);
}


void CGFFFormatter::EndSection(const CEndSectionItem&,
                               IFlatTextOStream& text_os)
{
    if ( !m_EndSequence.empty() ) {
        list<string> l;
        l.push_back(m_EndSequence);
        text_os.AddParagraph(l);
    }
}


void CGFFFormatter::FormatLocus
(const CLocusItem& locus, 
 IFlatTextOStream& text_os)
{
    m_Strandedness = locus.GetStrand();
}


void CGFFFormatter::FormatDate
(const CDateItem& date,
 IFlatTextOStream& text_os)
{
    m_Date.erase();

    const CDate* d = date.GetUpdateDate();
    if ( d != 0 ) {
        d->GetDate(&m_Date, "%4Y-%{%2M%|??%}-%{%2D%|??%}");
    }
}



///////////////////////////////////////////////////////////////////////////
//
// FEATURES


void CGFFFormatter::FormatFeature
(const CFeatureItemBase& f,
 IFlatTextOStream& text_os)
{
    CMappedFeat      seqfeat = f.GetFeat();
    string           key(f.GetKey()), oldkey;
    bool             gtf     = false;
    CBioseqContext& ctx = *f.GetContext();
    const CFlatFileConfig& cfg = ctx.Config();
    CScope* scope = &ctx.GetScope();

    // CSeq_loc         tentative_stop;

    if (( cfg.GffGTFCompat() )  &&  !ctx.IsProt()
        &&  (key == "CDS"  ||  key == "exon")) {
        gtf = true;
    } else if (( cfg.GffGTFCompat() )
               &&  ctx.GetMol() == CSeq_inst::eMol_dna
               &&  seqfeat.GetData().IsRna()) {
        oldkey = key;
        key    = "exon";
        gtf    = true;
    } else if ( cfg.GffGTFOnly() ) {
        return;
    }

    CConstRef<CFlatFeature> feat = f.Format();
    list<string>  l;
    list<string>  attr_list;

    if ( !oldkey.empty() ) {
        attr_list.push_back(x_FormatAttr("gbkey", oldkey));
    }

    ITERATE (CFlatFeature::TQuals, it, feat->GetQuals()) {
        string name = (*it)->GetName();
        if (name == "codon_start"  ||  name == "translation"
            ||  name == "transcription") {
            continue; // suppressed to reduce verbosity
        } else if (name == "number"  &&  key == "exon") {
            name = "exon_number";
        } else if (( cfg.GffGTFCompat() )  &&  !ctx.IsProt()
                   &&  name == "gene") {
            string gene_id = x_GetGeneID(*feat, (*it)->GetValue(), ctx);
            string transcript_id;
            if (key != "gene") {
                transcript_id = x_GetTranscriptID(*feat, gene_id, ctx);
            }
            x_AddGeneID(attr_list, gene_id, transcript_id);
            continue;
        } else if (name == "transcript_id") {
            name = "insd_transcript_id";
        }
        attr_list.push_back(x_FormatAttr(name, (*it)->GetValue()));
    }
    string attrs(NStr::Join(attr_list, x_GetAttrSep()));

    string source = x_GetSourceName(ctx);

    int frame = -1;
    if (seqfeat.GetData().IsCdregion()  &&  !ctx.IsProt() ) {
        const CCdregion& cds = seqfeat.GetData().GetCdregion();
        frame = max(cds.GetFrame() - 1, 0);
    }

    CConstRef<CSeq_loc> feat_loc(&f.GetLoc());
    CRef<CSeq_loc> tentative_stop;
    if (gtf  &&  seqfeat.GetData().IsCdregion()) {
        const CCdregion& cds = seqfeat.GetData().GetCdregion();
        if ( !f.GetLoc().IsPartialStop(eExtreme_Biological)  &&  seqfeat.IsSetProduct() ) {
            TSeqPos loc_len = sequence::GetLength(f.GetLoc(), scope);
            TSeqPos prod_len = sequence::GetLength(seqfeat.GetProduct(),
                                                   scope);
            if (loc_len >= frame + 3 * prod_len + 3) {
                SRelLoc::TRange range;
                range.SetFrom(frame + 3 * prod_len);
                range.SetTo  (frame + 3 * prod_len + 2);
                // needs to be partial for TranslateCdregion to DTRT
                range.SetFuzz_from().SetLim(CInt_fuzz::eLim_lt);
                SRelLoc::TRanges ranges;
                ranges.push_back(CRef<SRelLoc::TRange>(&range));
                tentative_stop = SRelLoc(f.GetLoc(), ranges).Resolve(scope);
            }
            if (tentative_stop.NotEmpty()  &&  !tentative_stop->IsNull()) {
                string s;
                CCdregion_translate::TranslateCdregion
                    (s, ctx.GetHandle(), *tentative_stop, cds);
                if (s != "*") {
                    tentative_stop.Reset();
                } else {
                    // valid stop, we may be able to trim the CDS
                    if (loc_len > frame + 3 * prod_len + 3) {
                        // truncation error
                        string msg("truncation error: ");
                        feature::GetLabel(seqfeat.GetOriginalFeature(), &msg,
                                          feature::fFGL_Both);
                        msg += "; protein: ";
                        seqfeat.GetProduct().GetLabel(&msg);

                        if (seqfeat.IsSetExcept()  &&
                            seqfeat.GetExcept()) {
                            msg = "warning: " + msg +
                                " (translation exception";
                            if (seqfeat.IsSetExcept_text()) {
                                msg += ' ' + seqfeat.GetExcept_text();
                            }
                            msg += ")";
                        } else {
                            msg = "error: " + msg;
                        }
                        LOG_POST_X(1, Error << msg);
                    } else {
                        SRelLoc::TRange range;
                        range.SetFrom(0);
                        range.SetTo(frame + 3 * prod_len - 1);
                        SRelLoc::TRanges ranges;
                        ranges.push_back(CRef<SRelLoc::TRange>(&range));
                        feat_loc = SRelLoc(f.GetLoc(), ranges).Resolve(scope);
                    }
                }
            } else {
                tentative_stop.Reset();
            }
        }
    }

    x_AddFeature(l, *feat_loc, source, key, "." /*score*/, frame, attrs,
                 gtf, ctx, tentative_stop.NotEmpty());

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
                tentative_start = SRelLoc(f.GetLoc(), ranges).Resolve(scope);
            }}

            string s;
            {{
                CSeqVector vect(*tentative_start, ctx.GetHandle().GetScope());
                vect.GetSeqData(0, 3, s);
            }}
            const CTrans_table* tt;
            if (cds.IsSetCode()) {
                tt = &CGen_code_table::GetTransTable(cds.GetCode());
            } else {
                tt = &CGen_code_table::GetTransTable(1);
            }
            if (s.size() == 3
                &&  tt->IsAnyStart(tt->SetCodonState(s[0], s[1], s[2]))) {
                x_AddFeature(l, *tentative_start, source, "start_codon",
                             "." /* score */, 0, attrs, gtf, ctx);
            }
        }
        if ( tentative_stop ) {
            x_AddFeature(l, *tentative_stop, source, "stop_codon",
                         "." /* score */, 0, attrs, gtf, ctx);
        }
    }

    text_os.AddParagraph(l, &seqfeat.GetOriginalFeature());
}


string CGFFFormatter::x_FormatAttr(const string& name, const string& value)
    const
{
    string value1;
    NStr::Replace(value, " \b", kEmptyStr, value1);
    string value2(NStr::PrintableString(value1));
    // some parsers may be dumb, so quote further
    value1.erase();
    ITERATE (string, c, value2) {
        switch (*c) {
        // Spaces allowed in the attribute column, if value is quoted,
        // which this function is already doing. Thus, avoid ugly \x20.
        // @see GTF specs v 1, 2, 2.1, 2.2
        // @see GFF version 1 and version 2
        // case ' ':  value1 += "\\x20"; break;
        case '\"': value1 += "x22";   break; // already backslashed
        case '#':  value1 += "\\x23"; break;
        default:   value1 += *c;
        }
    }
    return name + " \"" + value1 + "\";";
}


void CGFFFormatter::x_AddGeneID(list<string>& attr_list, const string& gene_id,
                                const string& transcript_id) const
{
    if ( !transcript_id.empty() ) {
        attr_list.push_front(x_FormatAttr("transcript_id", transcript_id));
    }
    attr_list.push_front(x_FormatAttr("gene_id", gene_id));
}


///////////////////////////////////////////////////////////////////////////
//
// BASE COUNT

// used as a trigger for the sequence header

void CGFFFormatter::FormatBasecount
(const CBaseCountItem& bc,
 IFlatTextOStream& text_os)
{
    CBioseqContext& ctx = *bc.GetContext();
    const CFlatFileConfig& cfg = ctx.Config();

    if ( ! ( cfg.GffShowSeq() ) )
        return;

    list<string> l;
    l.push_back("##" + m_SeqType + ' ' + ctx.GetAccession());
    text_os.AddParagraph(l);
    m_EndSequence = "##end-" + m_SeqType;
}


///////////////////////////////////////////////////////////////////////////
//
// SEQUENCE

void CGFFFormatter::FormatSequence
(const CSequenceItem& seq,
 IFlatTextOStream& text_os)
{
    CBioseqContext& ctx = *seq.GetContext();
    const CFlatFileConfig& cfg = ctx.Config();

    if ( ! ( cfg.GffShowSeq() ) )
        return;

    list<string> l;
    CSeqVector v = seq.GetSequence();
    v.SetCoding(CBioseq_Handle::eCoding_Iupac);

    CSeqVector_CI vi(v);
    string s;
    while (vi) {
        s.erase();
        vi.GetSeqData(s, 70);
        l.push_back("##" + s);
    }
    text_os.AddParagraph(l, ctx.GetHandle().GetCompleteBioseq());
}



// Private

string CGFFFormatter::x_GetGeneID(const CFlatFeature& feat,
                                  const string& gene,
                                  CBioseqContext& ctx) const
{
    //const CSeq_feat& seqfeat = feat.GetFeat();
    CMappedFeat seqfeat = feat.GetFeat();

    string main_acc = ctx.GetAccession();
    if (ctx.IsPart()) {
        const CSeq_id& id = *(ctx.GetMaster().GetHandle().GetSeqId());
        CSeq_id_Handle idh = ctx.GetPreferredSynonym(id);
        main_acc = idh.GetSeqId()->GetSeqIdString(true);
    }

    string gene_id = main_acc + ':' + gene;
    if (seqfeat.GetData().IsGene()) {
        return gene_id;
    }

    /**
    CConstRef<CSeq_feat> gene_feat =
        sequence::GetBestOverlappingFeat(seqfeat, CSeqFeatData::e_Gene,
                                         sequence::eOverlap_Interval,
                                         ctx.GetScope());
                                         **/
    CMappedFeat gene_feat =
        ctx.GetFeatTree().GetParent(seqfeat, CSeqFeatData::e_Gene);
    if (gene_feat) {
        gene_id = main_acc + ':';
        feature::GetLabel(gene_feat.GetOriginalFeature(), &gene_id,
                          feature::fFGL_Content);
    } else {
        string msg;
        feature::GetLabel(seqfeat.GetOriginalFeature(), &msg,
                          feature::fFGL_Both);
        LOG_POST_X(2, Info << "info: no best overlapping feature for " << msg);
    }

    return gene_id;
}



string CGFFFormatter::x_GetTranscriptID
(const CFlatFeature& feat,
 const string& gene_id,
 CBioseqContext& ctx) const
{
    //const CSeq_feat& seqfeat = feat.GetFeat();
    CMappedFeat seqfeat = feat.GetFeat();

    // if our feature already is an mRNA, we need look no further
    CMappedFeat rna_feat;
    switch (seqfeat.GetData().Which()) {
    case CSeqFeatData::e_Rna:
        rna_feat = seqfeat;
        break;

    case CSeqFeatData::e_Cdregion:
        if (seqfeat.GetData().GetSubtype() == CSeqFeatData::eSubtype_cdregion) {
            //rna_feat = sequence::GetBestMrnaForCds(seqfeat, ctx.GetScope());
            rna_feat = feature::GetBestMrnaForCds(seqfeat, &ctx.GetFeatTree());
        }
        break;

    default:
        break;
    }

    //
    // check if the mRNA feature we found has a product
    //
    if (rna_feat  &&  rna_feat.IsSetProduct()) {
        try {
            const CSeq_id& id = sequence::GetId(rna_feat.GetProduct(), 0);
            CSeq_id_Handle idh = ctx.GetPreferredSynonym(id);
            string transcript_id = idh.GetSeqId()->GetSeqIdString(true);
            return transcript_id;
        }
        catch (...) {
        }
    }

    //
    // nothing found, so fake it
    //

    // failed to get transcript id, so we fake a globally unique one based
    // on the gene id
    m_Transcripts[gene_id].push_back(seqfeat);

    string transcript_id = gene_id;
    transcript_id += ":unknown_transcript_";
    transcript_id += NStr::NumericToString(m_Transcripts[gene_id].size());
    return transcript_id;
}


string CGFFFormatter::x_GetSourceName(CBioseqContext& ctx) const
{
    // XXX - get from annot name (not presently available from IFF)?
    switch ( ctx.GetPrimaryId()->Which() ) {
    case CSeq_id::e_Local:                           return "Local";
    case CSeq_id::e_Gibbsq: case CSeq_id::e_Gibbmt:
    case CSeq_id::e_Giim:   case CSeq_id::e_Gi:      return "GenInfo";
    case CSeq_id::e_Genbank:                         return "Genbank";
    case CSeq_id::e_Swissprot:                       return "SwissProt";
    case CSeq_id::e_Patent:                          return "Patent";
    case CSeq_id::e_Other:                           return "RefSeq";
    case CSeq_id::e_General:
        return ctx.GetPrimaryId()->GetGeneral().GetDb();
    default:
    {
        string source
            (CSeq_id::SelectionName(ctx.GetPrimaryId()->Which()));
        return NStr::ToUpper(source);
    }
    }
}


void CGFFFormatter::x_AddFeature
(list<string>& l,
 const CSeq_loc& loc,
 const string& source,
 const string& key,
 const string& score,
 int frame,
 const string& attrs,
 bool gtf,
 CBioseqContext& ctx,
 bool tentative_stop) const
{
    int num_exons = 0;
    for (CSeq_loc_CI it(loc);  it;  ++it) {
        ++num_exons;
    }
    int exon_number = 1;
    for (CSeq_loc_CI it(loc);  it;  ++it) {
        TSeqPos from   = it.GetRange().GetFrom(), to = it.GetRange().GetTo();
        char    strand = '+';

        if (IsReverse(it.GetStrand())) {
            strand = '-';
        } else if (it.GetRange().IsWhole()) {
            strand = '.'; // N/A
        }

        if (it.GetRange().IsWhole()) {
            to = sequence::GetLength(it.GetSeq_id(), &ctx.GetScope()) - 1;
        }

        string extra_attrs;
        if (gtf  &&  attrs.find("exon_number") == NPOS) {
            CSeq_loc       loc2;
            CSeq_interval& si = loc2.SetInt();
            si.SetFrom(from);
            si.SetTo(to);
            si.SetStrand(it.GetStrand());
            si.SetId(const_cast<CSeq_id&>(it.GetSeq_id()));

            CConstRef<CSeq_feat> exon = sequence::GetBestOverlappingFeat
                (loc2, CSeqFeatData::eSubtype_exon,
                 sequence::eOverlap_Contains, ctx.GetScope());
            if (exon.NotEmpty()  &&  exon->IsSetQual()) {
				const CSeq_feat_Base::TQual & qual = exon->GetQual(); // must store reference since ITERATE macro evaluates 3rd arg multiple times
				ITERATE( CSeq_feat::TQual, q, qual ) {
                    if ( !NStr::CompareNocase((*q)->GetQual(), "number") ) {
                        int n = NStr::StringToNumeric((*q)->GetVal());
                        if (n >= exon_number) {
                            exon_number = n;
                            break;
                        }
                    }
                }
            }
            extra_attrs = x_GetAttrSep()
                + x_FormatAttr("exon_number", NStr::IntToString(exon_number));
            ++exon_number;
        }

        if ( sequence::IsSameBioseq(it.GetSeq_id(), *ctx.GetPrimaryId(),
                                    &ctx.GetScope()) ) {
            // conditionalize printing, but update state regardless
            l.push_back(ctx.GetAccession() + '\t'
                        + source + '\t'
                        + key + '\t'
                        + NStr::UIntToString(from + 1) + '\t'
                        + NStr::UIntToString(to + 1) + '\t'
                        + score + '\t'
                        + strand + '\t'
                        + (frame >= 0   &&  frame < 3 ? "021"[frame] : '.') + "\t"
                        + attrs + extra_attrs);
        }
        if (frame >= 0) {
            frame = (frame + to - from + 1) % 3;
        }
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
