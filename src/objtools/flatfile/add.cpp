/* add.c
 *
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
 * File Name:  add.c
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 * -----------------
 *      Additional parser functions.
 *
 */
#include <ncbi_pch.hpp>

#include <objtools/flatfile/ftacpp.hpp>

#include <objects/seq/Seq_gap.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seq_hist.hpp>
#include <objects/seq/Seq_hist_rec.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <objtools/flatfile/index.h>
#include <objtools/flatfile/indx_blk.h>
#include <objtools/flatfile/genbank.h>                    /* for ParFlat_FEATURES */
#include <objtools/flatfile/embl.h>                       /* for ParFlat_FH */
#include <objtools/flatfile/utilfun.h>

#include <objtools/flatfile/asci_blk.h>
#include <objtools/flatfile/flatdefn.h>
#include <objtools/flatfile/ftanet.h>

#ifdef THIS_FILE
#    undef THIS_FILE
#endif
#define THIS_FILE "add.cpp"

#define HTG_GAP   100
#define SHORT_GAP 20

typedef struct _seq_loc_ids {
    ncbi::objects::CSeq_loc* badslp;
    const Char*   wgsacc;
    const Char*   wgscont;
    const Char*   wgsscaf;
    Int4      genbank;
    Int4      embl;
    Int4      pir;
    Int4      swissprot;
    Int4      other;
    Int4      ddbj;
    Int4      prf;
    Int4      tpg;
    Int4      tpe;
    Int4      tpd;
    Int4      total;
} SeqLocIds, PNTR SeqLocIdsPtr;

typedef struct _fta_tpa_block {
    Int4                       from1;
    Int4                       to1;
    CharPtr                    accession;
    Int4                       version;
    Int4                       from2;
    Int4                       to2;
    Uint1                      strand;
    Uint1                      sicho;   /* SeqId choice */
    struct _fta_tpa_block PNTR next;
} FTATpaBlock, PNTR FTATpaBlockPtr;

typedef struct _fta_tpa_span {
    Int4                      from;
    Int4                      to;
    struct _fta_tpa_span PNTR next;
} FTATpaSpan, PNTR FTATpaSpanPtr;

/**********************************************************/
static void fta_tpa_block_free(FTATpaBlockPtr ftbp)
{
    FTATpaBlockPtr next;

    for(; ftbp != NULL; ftbp = next)
    {
        next = ftbp->next;
        if(ftbp->accession != NULL)
            MemFree(ftbp->accession);
        MemFree(ftbp);
    }
}

/**********************************************************
 *
 *   CharPtr tata_save(str):
 *
 *      Deletes spaces from the begining and the end and
 *   returns Nlm_StringSave.
 *
 **********************************************************/
CharPtr tata_save(CharPtr str)
{
    CharPtr s;
    CharPtr ss;

    if(str == NULL)
        return(NULL);

    while(isspace((int) *str) != 0 || *str == ',')
        str++;
    for(s = str; *s != '\0'; s++)
    {
        if(*s != '\n')
            continue;

        for(ss = s + 1; isspace((int) *ss) != 0;)
            ss++;
        *s = ' ';
        fta_StringCpy(s + 1, ss);
    }
    s = str + StringLen(str) - 1;
    while(s >= str && (*s == ' ' || *s == ';' || *s == ',' || *s == '\"' ||
                       *s == '\t'))
        *s-- = '\0';

    if(*str == '\0')
        return(NULL);

    return(StringSave(str));
}

/**********************************************************/
bool no_date(Int2 format, const TSeqdescList& descrs)
{
    bool no_create = true;
    bool no_update = true;

    ITERATE(TSeqdescList, desc, descrs)
    {
        if ((*desc)->IsCreate_date())
            no_create = false;
        else if ((*desc)->IsUpdate_date())
            no_update = false;

        if (no_create == false && no_update == false)
            break;
    }

    if(format == ParFlat_GENBANK)
        return(no_update);

    return(no_create || no_update);
}

/**********************************************************
 *
 *   bool no_reference(bsp):
 *
 *      Search for at least one reference in bioseq->desr
 *   or in bioseq->annot.
 *      If no reference return TRUE.
 *
 **********************************************************/
bool no_reference(const ncbi::objects::CBioseq& bioseq)
{
    ITERATE(TSeqdescList, desc, bioseq.GetDescr().Get())
    {
        if ((*desc)->IsPub())
            return false;
    }

    ITERATE(ncbi::objects::CBioseq::TAnnot, annot, bioseq.GetAnnot())
    {
        if (!(*annot)->IsFtable())
            continue;

        ITERATE(ncbi::objects::CSeq_annot::C_Data::TFtable, feat, (*annot)->GetData().GetFtable())
        {
            if ((*feat)->IsSetData() && (*feat)->GetData().IsPub())
                return false;
        }

        ITERATE(ncbi::objects::CSeq_annot::C_Data::TFtable, feat, (*annot)->GetData().GetFtable())
        {
            if (!(*feat)->IsSetData() || !(*feat)->GetData().IsImp())
                continue;

            const ncbi::objects::CImp_feat& imp = (*feat)->GetData().GetImp();
            if (imp.GetKey() == "Site-ref")
            {
                ErrPostStr(SEV_ERROR, ERR_REFERENCE_Illegalreference,
                           "The entry has only 'sites' references");
                return false;
            }
        }
    }

    return true;
}

/**********************************************************
 *
 *   bool check_cds(entry, format):
 *
 *      Returns TRUE if CDS is in the entry.
 *
 **********************************************************/
bool check_cds(DataBlkPtr entry, Int2 format)
{
    DataBlkPtr temp;
    DataBlkPtr dbp;
    const char *str;
    CharPtr    p;
    Char       ch;
    Int2       type;

    if(format == ParFlat_EMBL)
    {
        type = ParFlat_FH;
        str = "\nFT   CDS  ";
    }
    else if(format == ParFlat_GENBANK)
    {
        type = ParFlat_FEATURES;
        str = "\n     CDS  ";
    }
    else
        return(false);

    for(temp = TrackNodeType(entry, type); temp != NULL; temp = temp->next)
    {
        if(temp->type != type)
            continue;

        size_t len = 0;
        for(dbp = (DataBlkPtr) temp->data; dbp != NULL; dbp = dbp->next)
            len += dbp->len;
        if(len == 0)
            continue;

        dbp = (DataBlkPtr) temp->data;
        ch = dbp->offset[len];
        dbp->offset[len] = '\0';
        p = StringStr(dbp->offset, str);
        dbp->offset[len] = ch;

        if(p != NULL)
            break;
    }

    if(temp == NULL)
        return(false);
    return(true);
}

/**********************************************************/
void err_install(IndexblkPtr ibp, bool accver)
{
    Char temp[200];

    FtaInstallPrefix(PREFIX_LOCUS, ibp->locusname, NULL);
    if(accver && ibp->vernum > 0)
        sprintf(temp, "%s.%d", ibp->acnum, ibp->vernum);
    else
        StringCpy(temp, ibp->acnum);
    if(*temp == '\0')
        StringCpy(temp, ibp->locusname);
    FtaInstallPrefix(PREFIX_ACCESSION, temp, NULL);
}

/**********************************************************/
static void CreateSeqGap(ncbi::objects::CSeq_literal& seq_lit, GapFeatsPtr gfp)
{
    if (gfp == NULL)
        return;

    ncbi::objects::CSeq_gap& sgap = seq_lit.SetSeq_data().SetGap();
    sgap.SetType(gfp->asn_gap_type);

    if (!gfp->asn_linkage_evidence.empty())
        sgap.SetLinkage_evidence().swap(gfp->asn_linkage_evidence);

    if (StringCmp(gfp->gap_type, "unknown") == 0 ||
        StringCmp(gfp->gap_type, "within scaffold") == 0 ||
        StringCmp(gfp->gap_type, "repeat within scaffold") == 0)
        sgap.SetLinkage(1);
    else
        sgap.SetLinkage(0);
}

/**********************************************************/
void AssemblyGapsToDelta(ncbi::objects::CBioseq& bioseq, GapFeatsPtr gfp, Uint1Ptr drop)
{
    if (!bioseq.GetInst().IsSetExt() || !bioseq.GetInst().GetExt().IsDelta() ||
       gfp == NULL)
        return;

    ncbi::objects::CDelta_ext::Tdata& deltas = bioseq.SetInst().SetExt().SetDelta();
    ncbi::objects::CDelta_ext::Tdata::iterator delta = deltas.begin();
    for (; delta != deltas.end(); ++delta)
    {
        if (gfp == NULL)
            break;

        if (!(*delta)->IsLiteral())            /* not Seq-lit */
            continue;

        ncbi::objects::CSeq_literal& literal = (*delta)->SetLiteral();
        if (literal.GetLength() != static_cast<Uint4>(gfp->to - gfp->from + 1))
        {
            ErrPostEx(SEV_REJECT, ERR_FORMAT_ContigVersusAssemblyGapMissmatch,
                      "The lengths of the CONTIG/CO line gaps disagrees with the lengths of assembly_gap features. First assembly_gap with a mismatch is at \"%d..%d\".",
                      gfp->from, gfp->to);
            *drop = 1;
            break;
        }

        CreateSeqGap(literal, gfp);

        gfp = gfp->next;
    }

    if (*drop != 0 || (delta == deltas.end() && gfp == NULL))
        return;

    if (delta == deltas.end() && gfp != NULL)
    {
        ErrPostEx(SEV_REJECT, ERR_FORMAT_ContigVersusAssemblyGapMissmatch,
                  "The number of the assembly_gap features exceeds the number of CONTIG/CO line gaps. First extra assembly_gap is at \"%d..%d\".",
                  gfp->from, gfp->to);
        *drop = 1;
    }
    else if (delta != deltas.end() && gfp == NULL)
    {
        for (; delta != deltas.end(); ++delta)
        {
            if ((*delta)->IsLiteral())            /* Seq-lit */
                break;
        }

        if (delta == deltas.end())
            return;

        ErrPostEx(SEV_REJECT, ERR_FORMAT_ContigVersusAssemblyGapMissmatch,
                  "The number of the CONTIG/CO line gaps exceeds the number of assembly_gap features.");
        *drop = 1;
    }
}

/**********************************************************/
void GapsToDelta(ncbi::objects::CBioseq& bioseq, GapFeatsPtr gfp, Uint1Ptr drop)
{
    GapFeatsPtr  tgfp;

    const Char*  p;
    Int4         prevto;
    Int4         nextfrom;
    Int4         i;

    if (gfp == NULL || !bioseq.GetInst().IsSetSeq_data())
        return;

    const std::string& sequence = bioseq.GetInst().GetSeq_data().GetIupacna();
    p = sequence.c_str();

    if (sequence.empty() || sequence.size() != bioseq.GetLength())
        return;

    for(prevto = 0, tgfp = gfp; tgfp != NULL; tgfp = tgfp->next)
    {
        if(tgfp->next != NULL)
        {
            p = sequence.c_str() + tgfp->to;
            for(i = tgfp->to + 1; i < tgfp->next->from; p++, i++)
                if(*p != 'N')
                    break;
            if(i == tgfp->next->from && tgfp->next->from > tgfp->to + 1)
            {
                ErrPostEx(SEV_ERROR, ERR_FEATURE_AllNsBetweenGaps,
                          "A run of all-N sequence exists between the gap features located at \"%d..%d\" and \"%d..%d\".",
                          tgfp->from, tgfp->to, tgfp->next->from,
                          tgfp->next->to);
                tgfp->rightNs = true;
                tgfp->next->leftNs = true;
            }
            nextfrom = tgfp->next->from;
        }
        else
            nextfrom = bioseq.GetLength() + 1;

        if(tgfp->leftNs == false && tgfp->from - prevto > 10)
        {
            for (p = sequence.c_str() + tgfp->from - 11, i = 0; i < 10; p++, i++)
                if(*p != 'N')
                    break;
            if(i == 10)
            {
                ErrPostEx(SEV_WARNING, ERR_FEATURE_NsAbutGap,
                          "A run of N's greater or equal than 10 abuts the gap feature at \"%d..%d\" : possible problem with the boundaries of the gap.",
                          tgfp->from, tgfp->to);
            }
        }

        if(tgfp->rightNs == false && nextfrom - tgfp->to > 10)
        {
            for (p = sequence.c_str() + tgfp->to, i = 0; i < 10; p++, i++)
                if(*p != 'N')
                    break;
            if(i == 10)
            {
                ErrPostEx(SEV_WARNING, ERR_FEATURE_NsAbutGap,
                          "A run of N's greater or equal than 10 abuts the gap feature at \"%d..%d\" : possible problem with the boundaries of the gap.",
                          tgfp->from, tgfp->to);
            }
        }

        for (i = tgfp->from - 1, p = sequence.c_str() + i; i < tgfp->to; p++, i++)
            if(*p != 'N')
                break;
        if(i < tgfp->to)
        {
            ErrPostEx(SEV_REJECT, ERR_FEATURE_InvalidGapSequence,
                      "The sequence data associated with the gap feature at \"%d..%d\" contains basepairs other than N.",
                      tgfp->from, tgfp->to);
            *drop = 1;
        }

        prevto = tgfp->to;
    }

    if (*drop != 0)
        return;

    ncbi::objects::CDelta_ext::Tdata deltas;
        
    for (prevto = 0, tgfp = gfp;; tgfp = tgfp->next)
    {
        Int4 len = 0;

        ncbi::CRef<ncbi::objects::CDelta_seq> delta(new ncbi::objects::CDelta_seq);
        if (tgfp->from - prevto - 1 > 0)
        {
            len = tgfp->from - prevto - 1;
            delta->SetLiteral().SetLength(len);
            delta->SetLiteral().SetSeq_data().SetIupacna().Set() = sequence.substr(prevto, len);

            deltas.push_back(delta);

            delta.Reset(new ncbi::objects::CDelta_seq);
        }

        len = tgfp->to - tgfp->from + 1;
        delta->SetLiteral().SetLength(len);
        if(tgfp->estimated_length == -100)
        {
            delta->SetLiteral().SetFuzz().SetLim();
        }
        else if(tgfp->estimated_length != len)
        {
            delta->SetLiteral().SetFuzz().SetRange().SetMin(tgfp->estimated_length);
            delta->SetLiteral().SetFuzz().SetRange().SetMax(len);
        }

        if (tgfp->assembly_gap)
            CreateSeqGap(delta->SetLiteral(), tgfp);

        deltas.push_back(delta);

        prevto = tgfp->to;

        if(tgfp->next == NULL)
        {
            if (bioseq.GetLength() - prevto > 0)
            {
                delta.Reset(new ncbi::objects::CDelta_seq);

                len = bioseq.GetLength() - prevto;
                delta->SetLiteral().SetLength(len);
                delta->SetLiteral().SetSeq_data().SetIupacna().Set() = sequence.substr(prevto, len);

                deltas.push_back(delta);
            }
            break;
        }
    }

    if (!deltas.empty())
    {
        bioseq.SetInst().SetExt().SetDelta().Set().swap(deltas);
        bioseq.SetInst().SetRepr(ncbi::objects::CSeq_inst::eRepr_delta);
        bioseq.SetInst().ResetSeq_data();
    }
}

/**********************************************************/
void SeqToDelta(ncbi::objects::CBioseq& bioseq, Int2 tech)
{
    CharPtr  p;
    CharPtr  q;
    CharPtr  r;

    Int4         i;
    Int4         j;
    Int4         gotcha;

    if (!bioseq.GetInst().IsSetSeq_data())
        return;

    const std::string& sequence = bioseq.GetInst().GetSeq_data().GetIupacna();
    if (sequence.empty() || sequence.size() != bioseq.GetLength())
        return;

    vector<Char> buf(sequence.begin(), sequence.end());
    buf.push_back(0);
    p = &buf[0];
    gotcha = 0;

    ncbi::objects::CDelta_ext::Tdata deltas;

    for (q = p; *p != '\0';)
    {
        if(*p != 'N')
        {
            p++;
            continue;
        }

        for(r = p, p++, i = 1; *p == 'N'; i++)
            p++;
        if(i < HTG_GAP)
        {
            if(i >= SHORT_GAP && gotcha == 0)
                gotcha = 1;
            continue;
        }

        ncbi::CRef<ncbi::objects::CDelta_seq> delta(new ncbi::objects::CDelta_seq);
        gotcha = 2;

        if(r != q)
        {
            *r = '\0';
            j = (Int4) (r - q);

            delta->SetLiteral().SetLength(j);
            delta->SetLiteral().SetSeq_data().SetIupacna().Set(std::string(q, r));

            deltas.push_back(delta);

            delta.Reset(new ncbi::objects::CDelta_seq);

            *r = 'N';
        }

        delta->SetLiteral().SetLength(i);
        if (i == 100)
        {
            delta->SetLiteral().SetFuzz().SetLim();
        }

        deltas.push_back(delta);
        q = p;
    }

    if(p > q)
    {
        j = (Int4) (p - q);

        ncbi::CRef<ncbi::objects::CDelta_seq> delta(new ncbi::objects::CDelta_seq);
        delta->SetLiteral().SetLength(j);
        delta->SetLiteral().SetSeq_data().SetIupacna().Set(std::string(q, p));

        deltas.push_back(delta);
    }

    if (deltas.size() > 1)
    {
        bioseq.SetInst().SetExt().SetDelta().Set().swap(deltas);
        bioseq.SetInst().SetRepr(ncbi::objects::CSeq_inst::eRepr_delta);
        bioseq.SetInst().ResetSeq_data();
    }

    if (bioseq.GetInst().GetRepr() != ncbi::objects::CSeq_inst::eRepr_delta && tech == 1)
    {
        ErrPostEx(SEV_WARNING, ERR_SEQUENCE_HTGWithoutGaps,
                  "This Phase 1 HTG sequence has no runs of 100 "
                  "or more N's to indicate gaps between component contigs. "
                  "This could be an error, or perhaps sequencing is finished "
                  "and this record should not be Phase 1.");
    }

    if (bioseq.GetInst().GetRepr() == ncbi::objects::CSeq_inst::eRepr_delta)
    {
        if(tech == 4)                   /* Phase 0 */
            ErrPostEx(SEV_WARNING, ERR_SEQUENCE_HTGPhaseZeroHasGap,
                      "A Phase 0 HTG record usually consists of several reads "
                      "for one contig, and hence gaps are not expected. But "
                      "this record does have one (ore more) gaps, hence it "
                      "may require review.");
        if(gotcha == 1)
            ErrPostEx(SEV_WARNING, ERR_SEQUENCE_HTGPossibleShortGap,
                      "This sequence has one or more runs "
                      "of at least 20 N's. They could indicate gaps, "
                      "but have not been treated that way because "
                      "they are below the minimum of 100 N's.");
    }
}

/**********************************************************/
static bool fta_ranges_to_hist(const ncbi::objects::CGB_block::TExtra_accessions& extra_accs)
{
    std::string ppacc1;
    std::string ppacc2;
    CharPtr     master;
    CharPtr     range;
    CharPtr     acc1;
    CharPtr     acc2;
    CharPtr     p;
    CharPtr     q;
    Char        ch1;
    Char        ch2;
    Int4        i;

    if(extra_accs.empty())
        return(false);

    if(extra_accs.size() != 2)
        return(true);

    ncbi::objects::CGB_block::TExtra_accessions::const_iterator it = extra_accs.begin();

    ppacc1 = *it;
    ++it;
    ppacc2 = *it;
    acc1 = (CharPtr) ppacc1.c_str();
    acc2 = (CharPtr) ppacc2.c_str();


    if(acc1 == NULL && acc2 == NULL)
        return(FALSE);
    if(acc1 == NULL || acc2 == NULL)
        return(TRUE);

    p = StringChr(acc1, '-');
    q = StringChr(acc2, '-');

    if((p == NULL && q == NULL) || (p != NULL && q != NULL))
        return(TRUE);

    if(p == NULL)
    {
        master = acc1;
        range = acc2;
        *q = '\0';
    }
    else
    {
        master = acc2;
        range = acc1;
        *p = '\0';
    }

    if(fta_if_wgs_acc(master) != 0 || fta_if_wgs_acc(range) != 1)
    {
        if(p != NULL)
            *p = '-';
        if(q != NULL)
            *q = '-';
        return(true);
    }

    if(p != NULL)
        *p = '-';
    if(q != NULL)
        *q = '-';

    for(p = master; *p != '\0' && (*p < '0' || *p > '9');)
        p++;
    if(*p != '\0')
        p++;
    if(*p != '\0')
        p++;
    ch1 = *p;
    *p = '\0';

    for(q = range; *q != '\0' && (*q < '0' || *q > '9');)
        q++;
    if(*q != '\0')
        q++;
    if(*q != '\0')
        q++;
    ch2 = *q;
    *q = '\0';

    i = StringCmp(master, range);
    *p = ch1;
    *q = ch2;

    if(i == 0)
        return(false);
    return(true);
}

/**********************************************************/
void fta_add_hist(ParserPtr pp, ncbi::objects::CBioseq& bioseq, ncbi::objects::CGB_block::TExtra_accessions& extra_accs, Int4 source,
                  Int4 acctype, bool pricon, CharPtr acc)
{
    IndexblkPtr  ibp;

    CharPtr      p;
    CharPtr      acctmp;
    bool         good;
    Uint1        whose;
    Int4         pri_acc;
    Int4         sec_acc;
    size_t       i = 0;

    if(pp->accver == false || pp->histacc == false ||
       pp->source != source || pp->entrez_fetch == 0)
        return;

    if (!fta_ranges_to_hist(extra_accs))
        return;

    ncbi::objects::CGB_block::TExtra_accessions hist;
    UnwrapAccessionRange(extra_accs, hist);
    if (hist.empty())
        return;

    ibp = pp->entrylist[pp->curindx];

    acctmp = StringSave(acc);
    pri_acc = fta_if_wgs_acc(acctmp);
    if(pri_acc == 1)
    {
        for(p = acctmp; (*p >= 'A' && *p <= 'Z') || *p == '_';)
            p++;
        *p = '\0';
        i = StringLen(acctmp);
    }

    bool is_hist = bioseq.GetInst().IsSetHist();

    ncbi::objects::CSeq_hist_rec_Base::TIds& replaces = bioseq.SetInst().SetHist().SetReplaces().SetIds();

    ITERATE(ncbi::objects::CGB_block::TExtra_accessions, acc, hist)
    {
        if (acc->empty())
            continue;

        whose = GetNucAccOwner(acc->c_str(), NULL, ibp->is_tpa);
        sec_acc = fta_if_wgs_acc(acc->c_str());
        if(whose == 0 || sec_acc == 0)
            continue;

        if(sec_acc != 1)
            good = true;
        else
        {
            good = false;
            if(pri_acc == 1)
            {
                if (StringNCmp(acctmp, acc->c_str(), i) == 0 && (*acc)[i] >= '0' && (*acc)[i] <= '9')
                {
                    if(sec_acc == 1)
                        good = true;
                }
                else if(pp->allow_uwsec)
                    good = true;
            }
            else if(pri_acc != 2 && pri_acc != 0)
                good = true;
        }

        if(!good)
            continue;

        ncbi::CRef<ncbi::objects::CTextseq_id> text_id(new ncbi::objects::CTextseq_id);
        text_id->SetAccession(*acc);
        ncbi::CRef<ncbi::objects::CSeq_id> id(new ncbi::objects::CSeq_id);

        SetTextId(whose, *id, *text_id);

        if(pp->source != ParFlat_EMBL && pp->source != ParFlat_NCBI)
        {
            Int4 iscon = fta_is_con_div(pp, *id, acc->c_str());

            if(iscon < 0 || (iscon == 1 && pricon == false) ||
               (iscon == 0 && pricon && whose == acctype))
            {
                continue;
            }
        }

        replaces.push_back(id);
    }

    if (replaces.empty())
    {
        if (is_hist)
            bioseq.SetInst().SetHist().ResetReplaces();
        else
            bioseq.SetInst().ResetHist();
    }

    MemFree(acctmp);
}

/**********************************************************/
bool fta_strings_same(const Char PNTR s1, const Char PNTR s2)
{
    if(s1 == NULL && s2 == NULL)
        return true;
    if(s1 == NULL || s2 == NULL || StringCmp(s1, s2) != 0)
        return false;
    return true;
}

/**********************************************************/
bool fta_check_htg_kwds(TKeywordList& kwds, IndexblkPtr ibp, ncbi::objects::CMolInfo& mol_info)
{
    bool deldiv = false;

    for (TKeywordList::iterator key = kwds.begin(); key != kwds.end();)
    {
        bool delnode = false;
        bool errpost = false;
        if(*key == "HTGS_PHASE0")
        {
            if(ibp->htg != 0 && ibp->htg != 5)
            {
                delnode = true;
                if(ibp->htg == 1 || ibp->htg == 2 || ibp->htg == 3)
                    errpost = true;
            }
            else
            {
                ibp->htg = 4;
                mol_info.SetTech(ncbi::objects::CMolInfo::eTech_htgs_0);
            }
            deldiv = true;
        }
        else if (*key == "HTGS_PHASE1")
        {
            if(ibp->htg != 0 && ibp->htg != 5)
            {
                delnode = true;
                if(ibp->htg == 2 || ibp->htg == 3 || ibp->htg == 4)
                    errpost = true;
            }
            else
            {
                ibp->htg = 1;
                mol_info.SetTech(ncbi::objects::CMolInfo::eTech_htgs_1);
            }
            deldiv = true;
        }
        else if (*key == "HTGS_PHASE2")
        {
            if(ibp->htg != 0 && ibp->htg != 5)
            {
                delnode = true;
                if(ibp->htg == 1 || ibp->htg == 3 || ibp->htg == 4)
                    errpost = true;
            }
            else
            {
                ibp->htg = 2;
                mol_info.SetTech(ncbi::objects::CMolInfo::eTech_htgs_2);
            }
            deldiv = true;
        }
        else if (*key == "HTGS_PHASE3")
        {
            if(ibp->htg != 0 && ibp->htg != 5)
            {
                delnode = true;
                if(ibp->htg == 1 || ibp->htg == 2 || ibp->htg == 4)
                    errpost = true;
            }
            else
            {
                ibp->htg = 3;
                mol_info.SetTech(ncbi::objects::CMolInfo::eTech_htgs_3);
            }
            deldiv = true;
        }
        else if (*key == "HTG")
        {
            if(ibp->htg == 0)
            {
                ibp->htg = 5;
                mol_info.SetTech(ncbi::objects::CMolInfo::eTech_htgs_3);
            }
            deldiv = true;
        }

        if(errpost)
        {
            ErrPostEx(SEV_ERROR, ERR_KEYWORD_MultipleHTGPhases,
                      "This entry has multiple HTG-related keywords, for differing HTG phases. Ignoring all but the first.");
        }

        if (delnode)
            key = kwds.erase(key);
        else
            ++key;
    }
    if(ibp->htg == 5)
        ibp->htg = 3;

    return deldiv;
}

/**********************************************************/
static void fta_check_tpa_tsa_coverage(FTATpaBlockPtr ftbp, Int4 length, bool tpa)
{
    FTATpaBlockPtr tftbp;
    FTATpaSpanPtr  ftsp;
    FTATpaSpanPtr  tftsp;
    Int4           i1;
    Int4           i2;
    Int4           j;

    if(ftbp == NULL || length < 1)
        return;

    ftsp = (FTATpaSpanPtr) MemNew(sizeof(FTATpaSpan));
    ftsp->from = ftbp->from1;
    ftsp->to = ftbp->to1;
    ftsp->next = NULL;
    tftsp = ftsp;
    for(tftbp = ftbp; tftbp != NULL; tftbp = tftbp->next)
    {
        i1 = tftbp->to1 - tftbp->from1;
        i2 = tftbp->to2 - tftbp->from2;
        j = (i2 > i1) ? (i2 - i1) : (i1 - i2);
        i1++;

        if(i1 < 3000 && j * 10 > i1)
        {
            if(tpa)
                ErrPostEx(SEV_ERROR, ERR_TPA_SpanLengthDiff,
                "Span \"%d..%d\" of this TPA record differs from the span \"%d..%d\" of the contributing primary sequence or trace record by more than 10 percent.",
                tftbp->from1, tftbp->to1, tftbp->from2, tftbp->to2);
            else
                ErrPostEx(SEV_ERROR, ERR_TSA_SpanLengthDiff,
                          "Span \"%d..%d\" of this TSA record differs from the span \"%d..%d\" of the contributing primary sequence or trace record by more than 10 percent.",
                          tftbp->from1, tftbp->to1, tftbp->from2, tftbp->to2);
        }

        if(i1 >= 3000 && j > 300)
        {
            if (tpa)
                ErrPostEx(SEV_ERROR, ERR_TPA_SpanDiffOver300bp,
                "Span \"%d..%d\" of this TPA record differs from span \"%d..%d\" of the contributing primary sequence or trace record by more than 300 basepairs.",
                tftbp->from1, tftbp->to1, tftbp->from2, tftbp->to2);
            else
                ErrPostEx(SEV_ERROR, ERR_TSA_SpanDiffOver300bp,
                          "Span \"%d..%d\" of this TSA record differs from span \"%d..%d\" of the contributing primary sequence or trace record by more than 300 basepairs.",
                          tftbp->from1, tftbp->to1, tftbp->from2, tftbp->to2);
        }

        if(tftbp->from1 <= tftsp->to + 1)
        {
            if(tftbp->to1 > tftsp->to)
                tftsp->to = tftbp->to1;
            continue;
        }

        tftsp->next = (FTATpaSpanPtr) MemNew(sizeof(FTATpaSpan));
        tftsp = tftsp->next;
        tftsp->from = tftbp->from1;
        tftsp->to = tftbp->to1;
        tftsp->next = NULL;
    }

    if(ftsp->from - 1 > 50)
    {
        if(tpa)
            ErrPostEx(SEV_ERROR, ERR_TPA_IncompleteCoverage,
            "This TPA record contains a sequence region \"1..%d\" greater than 50 basepairs long that is not accounted for by a contributing primary sequence or trace record.",
            ftsp->from - 1);
        else
            ErrPostEx(SEV_ERROR, ERR_TSA_IncompleteCoverage,
                      "This TSA record contains a sequence region \"1..%d\" greater than 50 basepairs long that is not accounted for by a contributing primary sequence or trace record.",
                      ftsp->from - 1);
    }

    for(; ftsp != NULL; ftsp = tftsp)
    {
        tftsp = ftsp->next;
        if(tftsp != NULL && tftsp->from - ftsp->to - 1 > 50)
        {
            if(tpa)
                ErrPostEx(SEV_ERROR, ERR_TPA_IncompleteCoverage,
                "This TPA record contains a sequence region \"%d..%d\" greater than 50 basepairs long that is not accounted for by a contributing primary sequence or trace record.",
                ftsp->to + 1, tftsp->from - 1);
            else
                ErrPostEx(SEV_ERROR, ERR_TSA_IncompleteCoverage,
                          "This TSA record contains a sequence region \"%d..%d\" greater than 50 basepairs long that is not accounted for by a contributing primary sequence or trace record.",
                          ftsp->to + 1, tftsp->from - 1);
        }
        else if(tftsp == NULL && length - ftsp->to > 50)
        {
            if(tpa)
                ErrPostEx(SEV_ERROR, ERR_TPA_IncompleteCoverage,
                "This TPA record contains a sequence region \"%d..%d\" greater than 50 basepairs long that is not accounted for by a contributing primary sequence or trace record.",
                ftsp->to + 1, length);
            else
                ErrPostEx(SEV_ERROR, ERR_TSA_IncompleteCoverage,
                          "This TSA record contains a sequence region \"%d..%d\" greater than 50 basepairs long that is not accounted for by a contributing primary sequence or trace record.",
                          ftsp->to + 1, length);
        }

        MemFree(ftsp);
    }
}

/**********************************************************/
bool fta_number_is_huge(const Char* s)
{
    size_t i = StringLen(s);
    if(i > 10)
        return true;
    else if(i < 10)
        return false;

    if(*s > '2')
        return true;
    else if(*s < '2')
        return false;

    if(*++s > '1')
        return true;
    else if(*s < '1')
        return false;

    if(*++s > '4')
        return true;
    else if(*s < '4')
        return false;

    if(*++s > '7')
        return true;
    else if(*s < '7')
        return false;

    if(*++s > '4')
        return true;
    else if(*s < '4')
        return false;

    if(*++s > '8')
        return true;
    else if(*s < '8')
        return false;

    if(*++s > '3')
        return true;
    else if(*s < '3')
        return false;

    if(*++s > '6')
        return true;
    else if(*s < '6')
        return false;

    if(*++s > '4')
        return true;
    else if(*s < '4')
        return false;

    if(*++s > '7')
        return true;
    return false;
}

/**********************************************************/
bool fta_parse_tpa_tsa_block(ncbi::objects::CBioseq& bioseq, CharPtr offset, CharPtr acnum,
                             Int2 vernum, size_t len, Int2 col_data, bool tpa)
{
    FTATpaBlockPtr ftbp;
    FTATpaBlockPtr tftbp;
    FTATpaBlockPtr ft;

    CharPtr        buf;
    CharPtr        p;
    CharPtr        q;
    CharPtr        r;
    CharPtr        t;
    CharPtr        bad_accession;
    bool        bad_line;
    bool        bad_interval;
    Char           ch;
    Int4           from1;
    Int4           to1;
    Int4           len1;
    Int4           len2;
    Int2           i;
    Uint1          choice;

    if (offset == NULL || acnum == NULL || len < 2)
        return false;

    choice = GetNucAccOwner(acnum, &i, tpa);

    if(col_data == 0)                   /* HACK: XML format */
    {
        for(p = offset; *p != '\0'; p++)
            if(*p == '~')
                *p = '\n';
        p = StringChr(offset, '\n');
        if(p == NULL)
            return false;
        buf = (CharPtr) MemNew(StringLen(p) + 1);
        StringCpy(buf, p + 1);
        StringCat(buf, "\n");
    }
    else
    {
        ch = offset[len];
        offset[len] = '\0';
        p = StringChr(offset, '\n');
        if(p == NULL)
        {
            offset[len] = ch;
            return false;
        }
        buf = StringSave(p + 1);
        offset[len] = ch;
    }

    ftbp = (FTATpaBlockPtr) MemNew(sizeof(FTATpaBlock));

    bad_line = false;
    bad_interval = false;
    bad_accession = NULL;
    p = buf;
    for(q = StringChr(p, '\n'); q != NULL; p = q + 1, q = StringChr(p, '\n'))
    {
        *q = '\0';
        if((Int2) StringLen(p) < col_data)
            break;
        for(p += col_data; *p == ' ';)
            p++;
        for(r = p; *p >= '0' && *p <= '9';)
            p++;
        if(*p != '-')
        {
            bad_interval = true;
            break;
        }

        *p++ = '\0';
        from1 = atoi(r);

        for(r = p; *p >= '0' && *p <= '9';)
            p++;
        if(*p != ' ' && *p != '\n' && *p != '\0')
        {
            bad_interval = true;
            break;
        }
        if(*p != '\0')
            *p++ = '\0';
        to1 = atoi(r);

        if(from1 >= to1)
        {
            bad_interval = true;
            break;
        }

        for(ft = ftbp; ft->next != NULL; ft = ft->next)
            if((ft->next->from1 > from1) ||
               (ft->next->from1 == from1 && ft->next->to1 > to1))
                break;
        tftbp = (FTATpaBlockPtr) MemNew(sizeof(FTATpaBlock));
        tftbp->next = ft->next;
        ft->next = tftbp;

        tftbp->from1 = from1;
        tftbp->to1 = to1;

        while(*p == ' ')
            p++;
        for(r = p; *p != '\0' && *p != ' ' && *p != '\n';)
            p++;
        if(*p != '\0')
            *p++ = '\0';
        tftbp->accession = StringSave(r);
        r = StringChr(tftbp->accession, '.');
        if(r != NULL)
        {
            *r++ = '\0';
            for(t = r; *t >= '0' && *t <= '9';)
                t++;
            if(*t != '\0')
            {
                *--r = '.';
                bad_accession = tftbp->accession;
                break;
            }
            tftbp->version = atoi(r);
        }

        if(StringNICmp(tftbp->accession, "ti", 2) == 0)
        {
            for(r = tftbp->accession + 2; *r == '0';)
                r++;
            if(*r == '\0')
            {
                bad_accession = tftbp->accession;
                break;
            }
            while(*r >= '0' && *r <= '9')
                r++;
            if(*r != '\0')
            {
                bad_accession = tftbp->accession;
                break;
            }
        }
        else
        {
            i = 0;
            tftbp->sicho = GetNucAccOwner(tftbp->accession, &i, false);
            if ((tftbp->sicho != ncbi::objects::CSeq_id::e_Genbank && tftbp->sicho != ncbi::objects::CSeq_id::e_Embl &&
                tftbp->sicho != ncbi::objects::CSeq_id::e_Ddbj &&
                (tftbp->sicho != ncbi::objects::CSeq_id::e_Tpg || tpa == false)) || i < 1)
            {
                bad_accession = tftbp->accession;
                break;
            }
        }

        while(*p == ' ')
            p++;

        if(StringNICmp(p, "not_available", 13) == 0)
        {
            p += 13;
            tftbp->from2 = 1;
            tftbp->to2 = 1;
        }
        else
        {
            for(r = p; *p >= '0' && *p <= '9';)
                p++;
            if(*p != '-')
            {
                bad_interval = true;
                break;
            }
            *p++ = '\0';
            tftbp->from2 = atoi(r);

            for(r = p; *p >= '0' && *p <= '9';)
                p++;
            if(*p != ' ' && *p != '\n' && *p != '\0')
            {
                bad_interval = true;
                break;
            }
            if(*p != '\0')
                *p++ = '\0';
            tftbp->to2 = atoi(r);

            if(tftbp->from2 >= tftbp->to2)
            {
                bad_interval = true;
                break;
            }
        }

        while(*p == ' ')
            p++;
        if(*p == 'c')
        {
            tftbp->strand = 2;
            for(p++; *p == ' ';)
                p++;
        }
        else
            tftbp->strand = 1;
        if(*p != '\0')
        {
            bad_line = true;
            break;
        }
    }

    MemFree(buf);
    if (bad_line || bad_interval || bad_accession != NULL)
    {
        if(bad_interval)
        {
            if(tpa)
                ErrPostEx(SEV_REJECT, ERR_TPA_InvalidPrimarySpan,
                "Intervals from primary records on which a TPA record is based must be of form X-Y, where X is less than Y and both X and Y are integers. Entry dropped.");
            else
                ErrPostEx(SEV_REJECT, ERR_TSA_InvalidPrimarySpan,
                          "Intervals from primary records on which a TSA record is based must be of form X-Y, where X is less than Y and both X and Y are integers. Entry dropped.");
        }
        else if(bad_accession != NULL)
        {
            if(tpa)
                ErrPostEx(SEV_REJECT, ERR_TPA_InvalidPrimarySeqId,
                "\"%s\" is not a GenBank/EMBL/DDBJ/Trace sequence identifier. Entry dropped.",
                bad_accession);
            else
                ErrPostEx(SEV_REJECT, ERR_TSA_InvalidPrimarySeqId,
                          "\"%s\" is not a GenBank/EMBL/DDBJ/Trace sequence identifier. Entry dropped.",
                          bad_accession);
        }
        else
        {
            if(tpa)
                ErrPostEx(SEV_REJECT, ERR_TPA_InvalidPrimaryBlock,
                "Supplied PRIMARY block for TPA record is incorrect. Cannot parse. Entry dropped.");
            else
                ErrPostEx(SEV_REJECT, ERR_TSA_InvalidPrimaryBlock,
                          "Supplied PRIMARY block for TSA record is incorrect. Cannot parse. Entry dropped.");
        }

        if(ftbp != NULL)
            fta_tpa_block_free(ftbp);
        return false;
    }

    tftbp = ftbp->next;
    ftbp->next = NULL;
    MemFree(ftbp);
    ftbp = tftbp;

    fta_check_tpa_tsa_coverage(ftbp, bioseq.GetLength(), tpa);

    ncbi::objects::CSeq_hist::TAssembly& assembly = bioseq.SetInst().SetHist().SetAssembly();
    if (!assembly.empty())
        assembly.clear();

    ncbi::CRef<ncbi::objects::CSeq_align> root_align(new ncbi::objects::CSeq_align);

    root_align->SetType(ncbi::objects::CSeq_align::eType_not_set);
    ncbi::objects::CSeq_align_set& align_set = root_align->SetSegs().SetDisc();

    for(; tftbp != NULL; tftbp = tftbp->next)
    {
        len1 = tftbp->to1 - tftbp->from1 + 1;
        len2 = tftbp->to2 - tftbp->from2 + 1;

        ncbi::CRef<ncbi::objects::CSeq_align> align(new ncbi::objects::CSeq_align);
        align->SetType(ncbi::objects::CSeq_align::eType_partial);
        align->SetDim(2);

        ncbi::objects::CSeq_align::C_Segs::TDenseg& seg = align->SetSegs().SetDenseg();

        seg.SetDim(2);
        seg.SetNumseg((len1 == len2) ? 1 : 2);

        seg.SetStarts().push_back(tftbp->from1 - 1);
        seg.SetStarts().push_back(tftbp->from2 - 1);

        if (len1 != len2)
        {
            if (len1 < len2)
            {
                seg.SetStarts().push_back(-1);
                seg.SetStarts().push_back(tftbp->from2 - 1 + len1);
            }
            else
            {
                seg.SetStarts().push_back(tftbp->from1 - 1 + len2);
                seg.SetStarts().push_back(-1);
            }
        }

        if (len1 == len2)
            seg.SetLens().push_back(len1);
        else if(len1 < len2)
        {
            seg.SetLens().push_back(len1);
            seg.SetLens().push_back(len2 - len1);
        }
        else
        {
            seg.SetLens().push_back(len2);
            seg.SetLens().push_back(len1 - len2);
        }

        seg.SetStrands().push_back(ncbi::objects::eNa_strand_plus);
        seg.SetStrands().push_back(static_cast<ncbi::objects::ENa_strand>(tftbp->strand));

        if (len1 != len2)
        {
            seg.SetStrands().push_back(ncbi::objects::eNa_strand_plus);
            seg.SetStrands().push_back(static_cast<ncbi::objects::ENa_strand>(tftbp->strand));
        }

        ncbi::CRef<ncbi::objects::CTextseq_id> text_id(new ncbi::objects::CTextseq_id);
        text_id->SetAccession(acnum);

        if(vernum > 0)
            text_id->SetVersion(vernum);

        ncbi::CRef<ncbi::objects::CSeq_id> id(new ncbi::objects::CSeq_id),
                                           aux_id;
        SetTextId(choice, *id, *text_id);
        seg.SetIds().push_back(id);

        if(StringNICmp(tftbp->accession, "ti", 2) == 0)
        {
            ncbi::CRef<ncbi::objects::CSeq_id> gen_id(new ncbi::objects::CSeq_id);
            ncbi::objects::CDbtag& tag = gen_id->SetGeneral();

            for(r = tftbp->accession + 2; *r == '0';)
                r++;
            if(fta_number_is_huge(r) == false)
                tag.SetTag().SetId(atoi(r));
            else
                tag.SetTag().SetStr(r);

            tag.SetDb("ti");
            seg.SetIds().push_back(gen_id);
        }
        else
        {
            ncbi::CRef<ncbi::objects::CTextseq_id> otext_id(new ncbi::objects::CTextseq_id);
            otext_id->SetAccession(tftbp->accession);

            if (tftbp->version > 0)
                otext_id->SetVersion(tftbp->version);

            aux_id.Reset(new ncbi::objects::CSeq_id);
            SetTextId(tftbp->sicho, *aux_id, *otext_id);
        }

        if (aux_id.NotEmpty())
            seg.SetIds().push_back(aux_id);

        align_set.Set().push_back(align);
    }

    assembly.push_back(root_align);

    if(ftbp != NULL)
        fta_tpa_block_free(ftbp);
    return true;
}

/**********************************************************/
CharPtr StringRStr(CharPtr where, const char *what)
{
    if(where == NULL || what == NULL || *where == '\0' || *what == '\0')
        return(NULL);

    size_t i = StringLen(what);
    CharPtr res = nullptr;
    for(CharPtr p = where; *p != '\0'; p++)
        if(StringNCmp(p, what, i) == 0)
            res = p;

    return(res);
}

/**********************************************************/
ncbi::CRef<ncbi::objects::CSeq_loc> fta_get_seqloc_int_whole(ncbi::objects::CSeq_id& seq_id, size_t len)
{
    ncbi::CRef<ncbi::objects::CSeq_loc> ret;

    if (len < 1)
        return ret;

    ret.Reset(new ncbi::objects::CSeq_loc);
    ncbi::objects::CSeq_interval& interval = ret->SetInt();

    interval.SetFrom(0);
    interval.SetTo(static_cast<ncbi::TSeqPos>(len) - 1);
    interval.SetId(seq_id);

    return ret;
}

/**********************************************************/
static void fta_validate_assembly(CharPtr name)
{
    bool bad_format = false;

    CharPtr p = name;
    if(p == NULL || *p == '\0' || StringLen(p) < 7)
        bad_format = true;
    else if(p[0] != 'G' || p[1] != 'C' || (p[2] != 'F' && p[2] != 'A') ||
            p[3] != '_' || p[4] < '0' || p[4] > '9')
        bad_format = true;
    else
    {
        for(p += 5; *p != '\0'; p++)
            if(*p < '0' || *p > '9')
                break;
        if(*p != '.' || p[1] < '0' || p[1] > '9')
            bad_format = true;
        else
        {
            for(p++; *p != '\0'; p++)
                if(*p < '0' || *p > '9')
                    break;
            if(*p != '\0')
                bad_format = true;
        }
    }

    if(bad_format)
        ErrPostEx(SEV_WARNING, ERR_DBLINK_InvalidIdentifier,
                  "\"%s\" is not a validly formatted identifier for the Assembly resource.",
                  name);
}

/**********************************************************/
static bool fta_validate_bioproject(CharPtr name, Int2 source)
{
    CharPtr p;
    bool bad_format = false;

    if(StringLen(name) < 6)
        bad_format = true;
    else if(name[0] != 'P' || name[1] != 'R' || name[2] != 'J' ||
            (name[3] != 'E' && name[3] != 'N' && name[3] != 'D') ||
            name[4] < 'A' || name[4] > 'Z' || name[5] < '0' || name[5] > '9')
        bad_format = true;
    else
    {
        for(p = name + 6; *p != '\0'; p++)
            if(*p < '0' || *p > '9')
                break;
        if(*p != '\0')
            bad_format = true;
    }

    if(bad_format)
    {
        ErrPostEx(SEV_REJECT, ERR_FORMAT_InvalidBioProjectAcc,
                  "BioProject accession number is not validly formatted: \"%s\". Entry dropped.",
                  name);
        return false;
    }

    if((source == ParFlat_NCBI && name[3] != 'N') ||
       (source == ParFlat_DDBJ && name[3] != 'D' &&
        (name[3] != 'N' || name[4] != 'A')) ||
       (source == ParFlat_EMBL && name[3] != 'E' &&
        (name[3] != 'N' || name[4] != 'A')))
        ErrPostEx(SEV_WARNING, ERR_FORMAT_WrongBioProjectPrefix,
                  "BioProject accession number does not agree with this record's database of origin: \"%s\".",
                  name);

    return true;
}

/**********************************************************/
static ValNodePtr fta_tokenize_project(CharPtr str, Int2 source, bool newstyle)
{
    ValNodePtr vnp;
    ValNodePtr tvnp;
    CharPtr    p;
    CharPtr    q;
    CharPtr    r;
    bool    bad;
    Char       ch;

    if(str == NULL || *str == '\0')
    {
        ErrPostEx(SEV_REJECT, ERR_FORMAT_InvalidBioProjectAcc,
                  "Empty PROJECT/PR line type supplied. Entry dropped.");
        return(NULL);
    }

    for(p = str; *p != '\0'; p++)
        if(*p == ';' || *p == ',' || *p == '\t')
            *p = ' ';

    for(p = str; *p == ' ';)
        p++;
    if(*p == '\0')
    {
        ErrPostEx(SEV_REJECT, ERR_FORMAT_InvalidBioProjectAcc,
                  "Empty PROJECT/PR line type supplied. Entry dropped.");
        return(NULL);
    }

    vnp = ValNodeNew(NULL);
    vnp->data.ptrvalue = NULL;
    vnp->next = NULL;
    tvnp = vnp;

    for(bad = false, p = str; *p != '\0';)
    {
        while(*p == ' ')
            p++;

        if(*p == '\0')
            break;

        for(q = p; *p != ' ' && *p != '\0';)
            p++;

        ch = *p;
        *p = '\0';
        if(!newstyle)
        {
            for(r = q; *r >= '0' && *r <= '9';)
                r++;
            if(*r != '\0')
            {
                ErrPostEx(SEV_REJECT, ERR_FORMAT_InvalidBioProjectAcc,
                          "BioProject accession number is not validly formatted: \"%s\". Entry dropped.",
                          q);
                bad = true;
            }
        }
        else if(fta_validate_bioproject(q, source) == false)
            bad = true;

        if(bad)
        {
            *p = ch;
            break;
        }

        tvnp->next = ValNodeNew(NULL);
        tvnp = tvnp->next;
        tvnp->next = NULL;
        tvnp->data.ptrvalue = StringSave(q);

        *p = ch;
    }

    tvnp = vnp->next;
    MemFree(vnp);

    if(tvnp == NULL)
        return(NULL);

    if(!bad)
        return(tvnp);

    ValNodeFreeData(tvnp);
    return(NULL);
}

/**********************************************************/
void fta_get_project_user_object(TSeqdescList& descrs, CharPtr offset,
                                 Int2 format, Uint1Ptr drop,
                                 Int2 source)
{
    ValNodePtr    vnp;
    ValNodePtr    tvnp;

    const Char    *name;

    CharPtr       str;
    CharPtr       p;
    Char          ch;
    Int4          i;

    if(offset == NULL)
        return;

    bool newstyle = false;
    if(format == ParFlat_GENBANK)
    {
        i = ParFlat_COL_DATA;
        name = "GenomeProject:";
        ch = '\n';
    }
    else
    {
        i = ParFlat_COL_DATA_EMBL;
        name = "Project:";
        ch = ';';
    }

    size_t len = StringLen(name);
    str = StringSave(offset + i);
    p = StringChr(str, ch);
    if(p != NULL)
        *p = '\0';

    if(StringNCmp(str, name, len) != 0)
    {
        if(format == ParFlat_GENBANK)
        {
            ErrPostEx(SEV_REJECT, ERR_FORMAT_InvalidBioProjectAcc,
                      "PROJECT line is missing \"GenomeProject:\" tag. Entry dropped.",
                      str);
            MemFree(str);
            *drop = 1;
            return;
        }
        newstyle = true;
        len = 0;
    }
    else if(format == ParFlat_EMBL && str[len] == 'P')
        newstyle = true;

    vnp = fta_tokenize_project(str + len, source, newstyle);
    if(vnp == NULL)
    {
        *drop = 1;
        MemFree(str);
        return;
    }

    ncbi::objects::CUser_object* user_obj_ptr;
    bool got = false;

    NON_CONST_ITERATE(TSeqdescList, descr, descrs)
    {
        if (!(*descr)->IsUser() || !(*descr)->GetUser().IsSetData())
            continue;

        user_obj_ptr = &((*descr)->SetUser());

        ncbi::objects::CObject_id* obj_id = nullptr;
        if (user_obj_ptr->IsSetType())
            obj_id = &(user_obj_ptr->SetType());

        if (obj_id != NULL && obj_id->IsStr() && obj_id->GetStr() == "DBLink")
        {
            got = true;
            break;
        }
    }
        
    ncbi::CRef<ncbi::objects::CUser_object> user_obj;
    if (newstyle)
    {
        for(i = 0, tvnp = vnp; tvnp != NULL; tvnp = tvnp->next)
            i++;

        if (!got)
        {
            user_obj.Reset(new ncbi::objects::CUser_object);
            user_obj_ptr = user_obj.GetNCPointer();

            ncbi::objects::CObject_id& id = user_obj_ptr->SetType();
            id.SetStr("DBLink");
        }

        ncbi::CRef<ncbi::objects::CUser_field> user_field(new ncbi::objects::CUser_field);
        user_field->SetLabel().SetStr("BioProject");
        user_field->SetNum(i);

        for(tvnp = vnp, i = 0; tvnp != NULL; tvnp = tvnp->next)
            user_field->SetData().SetStrs().push_back((CharPtr)tvnp->data.ptrvalue);

        user_obj_ptr->SetData().push_back(user_field);
    }
    else
    {
        got = false;

        user_obj.Reset(new ncbi::objects::CUser_object);
        user_obj_ptr = user_obj.GetNCPointer();

        ncbi::objects::CObject_id& id = user_obj_ptr->SetType();
        id.SetStr("GenomeProjectsDB");

        for(tvnp = vnp; tvnp != NULL; tvnp = tvnp->next)
        {

            ncbi::CRef<ncbi::objects::CUser_field> user_field(new ncbi::objects::CUser_field);
            user_field->SetLabel().SetStr("ProjectID");
            user_field->SetData().SetInt(atoi((CharPtr)tvnp->data.ptrvalue));
            user_obj_ptr->SetData().push_back(user_field);


            user_field.Reset(new ncbi::objects::CUser_field);
            user_field->SetLabel().SetStr("ParentID");
            user_field->SetData().SetInt(0);
            user_obj_ptr->SetData().push_back(user_field);
        }
    }

    if (!got)
    {
        ncbi::CRef<ncbi::objects::CSeqdesc> descr(new ncbi::objects::CSeqdesc);
        descr->SetUser(*user_obj_ptr);
        descrs.push_back(descr);
    }

    MemFree(str);
    ValNodeFree(vnp);
}

/**********************************************************/
bool fta_if_valid_sra(const Char* id, bool dblink)
{
    const Char* p = id;

    if(p != NULL && StringLen(p) > 3 &&
       (p[0] == 'E' || p[0] == 'S' || p[0] == 'D') && p[1] == 'R' &&
       (p[2] == 'A' || p[2] == 'P' || p[2] == 'R' || p[2] == 'S' ||
        p[2] == 'X' || p[2] == 'Z'))
    {
        for(p += 3; *p >= '0' && *p <= '9';)
            p++;
        if(*p == '\0')
            return true;
    }

    if(dblink)
        ErrPostEx(SEV_REJECT, ERR_FORMAT_IncorrectDBLINK,
                  "Incorrectly formatted DBLINK Sequence Read Archive value: \"%s\". Entry dropped.",
                  id);

    return false;
}

/**********************************************************/
bool fta_if_valid_biosample(const Char* id, bool dblink)
{
    const Char* p = id;

    if(p != NULL && StringLen(p) > 5 && p[0] == 'S' && p[1] == 'A' &&
       p[2] == 'M' && (p[3] == 'N' || p[3] == 'E' || p[3] == 'D'))
    {
        if(p[4] == 'A' || p[4] == 'G')
            p += 5;
        else
            p += 4;
        while(*p >= '0' && *p <= '9')
            p++;
        if(*p == '\0')
            return true;
    }

    if(dblink)
        ErrPostEx(SEV_REJECT, ERR_FORMAT_IncorrectDBLINK,
                  "Incorrectly formatted DBLINK BioSample value: \"%s\". Entry dropped.",
                  id);

    return false;
}

/**********************************************************/
static ValNodePtr fta_tokenize_dblink(CharPtr str, Int2 source)
{
    ValNodePtr vnp;
    ValNodePtr tvnp;
    ValNodePtr uvnp;
    ValNodePtr tagvnp;

    bool    got_nl;
    bool    bad;
    bool    sra;
    bool    assembly;
    bool    biosample;
    bool    bioproject;

    CharPtr    p;
    CharPtr    q;
    CharPtr    r = NULL;
    CharPtr    t;
    CharPtr    u;
    Char       ch;

    if(str == NULL || *str == '\0')
    {
        ErrPostEx(SEV_REJECT, ERR_FORMAT_IncorrectDBLINK,
                  "Empty DBLINK line type supplied. Entry dropped.");
        return(NULL);
    }

    for(p = str; *p != '\0'; p++)
        if(*p == ';' || *p == '\t')
            *p = ' ';

    vnp = ValNodeNew(NULL);
    vnp->data.ptrvalue = NULL;
    tvnp = vnp;
    bad = false;
    got_nl = true;
    sra = false;
    assembly = false;
    biosample = false;
    bioproject = false;
    tagvnp = NULL;
    for(p = str; *p != '\0'; got_nl = false)
    {
        while(*p == ' ' || *p == '\n' || *p == ':' || *p == ',')
        {
            if(*p == '\n')
               got_nl = true;
            p++;
        }

        if(got_nl)
        {
            t = StringChr(p, ':');
            if(t != NULL)
            {
                r = StringChr(p, '\n');
                u = StringChr(p, ',');

                if((u == NULL || u > t) && (r == NULL || r > t))
                {
                    ch = *++t;
                    *t = '\0';

                    if(StringCmp(p, "Project:") != 0 &&
                       StringCmp(p, "Assembly:") != 0 &&
                       StringCmp(p, "BioSample:") != 0 &&
                       StringCmp(p, "BioProject:") != 0 &&
                       StringCmp(p, "Sequence Read Archive:") != 0 &&
                       StringCmp(p, "Trace Assembly Archive:") != 0)
                    {
                        ErrPostEx(SEV_REJECT, ERR_FORMAT_IncorrectDBLINK,
                                  "Invalid DBLINK tag encountered: \"%s\". Entry dropped.", p);
                        bad = true;
                        break;
                    }

                    bioproject = (StringCmp(p, "BioProject:") == 0);
                    sra = (StringCmp(p, "Sequence Read Archive:") == 0);
                    biosample = (StringCmp(p, "BioSample:") == 0);
                    assembly = (StringCmp(p, "Assembly:") == 0);

                    if(tvnp->data.ptrvalue != NULL &&
                       StringChr((CharPtr) tvnp->data.ptrvalue, ':') != NULL)
                    {
                        ErrPostEx(SEV_REJECT, ERR_FORMAT_IncorrectDBLINK,
                                  "Found DBLINK tag with no value: \"%s\". Entry dropped.",
                                  tvnp->data.ptrvalue);
                        bad = true;
                        break;
                    }

                    for(uvnp = vnp->next; uvnp != NULL; uvnp = uvnp->next)
                        if(StringCmp((CharPtr) uvnp->data.ptrvalue, p) == 0)
                    {
                        ErrPostEx(SEV_REJECT, ERR_FORMAT_IncorrectDBLINK,
                                  "Multiple DBLINK tags found: \"%s\". Entry dropped.",
                                  p);
                        bad = true;
                        break;
                    }
                    if(bad)
                        break;

                    tvnp->next = ValNodeNew(NULL);
                    tvnp = tvnp->next;
                    tvnp->next = NULL;
                    tvnp->data.ptrvalue = StringSave(p);
                    tagvnp = tvnp;
                    *t = ch;
                    p = t;
                    continue;
                }
            }
        }

        q = p;
        while(*p != ',' && *p != '\n' && *p != ':' && *p != '\0')
            p++;
        if(*p == ':')
        {
            while(*p != '\0' && *p != '\n')
                p++;
            ch = *p;
            *p = '\0';
            while(*r != '\n' && r > str)
                r--;
            while(*r == ' ' || *r == '\n')
                r++;
            ErrPostEx(SEV_REJECT, ERR_FORMAT_IncorrectDBLINK,
                      "Too many delimiters/fields for DBLINK line: \"%s\". Entry dropped.",
                      r);
            *p = ch;
            bad = true;
            break;
        }

        if(q == p)
            continue;

        ch = *p;
        *p = '\0';

        if(tagvnp != NULL && tagvnp->data.ptrvalue != NULL)
        {
            for(uvnp = tagvnp->next; uvnp != NULL; uvnp = uvnp->next)
            {
                if(uvnp->data.ptrvalue == NULL ||
                   StringCmp((CharPtr) uvnp->data.ptrvalue, q) != 0)
                    continue;

                ErrPostEx(SEV_WARNING, ERR_DBLINK_DuplicateIdentifierRemoved,
                          "Duplicate identifier \"%s\" from \"%s\" link removed.",
                          q, (CharPtr) tagvnp->data.ptrvalue);
                break;
            }

            if(uvnp != NULL)
            {
                *p = ch;
                continue;
            }
        }

        if((bioproject &&
            fta_validate_bioproject(q, source) == false) ||
           (biosample && fta_if_valid_biosample(q, true) == false) ||
           (sra && fta_if_valid_sra(q, true) == false))
        {
            *p = ch;
            bad = true;
        }

        if(assembly)
           fta_validate_assembly(q);

        tvnp->next = ValNodeNew(NULL);
        tvnp = tvnp->next;
        tvnp->next = NULL;
        tvnp->data.ptrvalue = StringSave(q);
        *p = ch;
    }

    if(!bad && tvnp->data.ptrvalue != NULL &&
       StringChr((CharPtr) tvnp->data.ptrvalue, ':') != NULL)
    {
        ErrPostEx(SEV_REJECT, ERR_FORMAT_IncorrectDBLINK,
                  "Found DBLINK tag with no value: \"%s\". Entry dropped.",
                  tvnp->data.ptrvalue);
        bad = true;
    }

    tvnp = vnp->next;
    MemFree(vnp);

    if(tvnp == NULL)
        return(NULL);

    if(!bad)
        return(tvnp);

    ValNodeFreeData(tvnp);
    return(NULL);
}

/**********************************************************/
void fta_get_dblink_user_object(TSeqdescList& descrs, CharPtr offset,
                                size_t len, Int2 source, Uint1Ptr drop,
                                ncbi::CRef<ncbi::objects::CUser_object>& dbuop)
{
    ValNodePtr    vnp;
    ValNodePtr    tvnp;
    ValNodePtr    uvnp;

    CharPtr       str;
    Int4          i;

    if(offset == NULL)
        return;

    str = StringSave(offset + ParFlat_COL_DATA);
    str[len-ParFlat_COL_DATA] = '\0';
    vnp = fta_tokenize_dblink(str, source);
    MemFree(str);

    if(vnp == NULL)
    {
        *drop = 1;
        return;
    }

    ncbi::CRef<ncbi::objects::CUser_object> user_obj;
    ncbi::CRef<ncbi::objects::CUser_field> user_field;

    for (tvnp = vnp; tvnp != NULL; tvnp = tvnp->next)
    {
        if(StringChr((CharPtr) tvnp->data.ptrvalue, ':') != NULL)
        {
            if (user_obj.NotEmpty())
                break;

            if(StringCmp((CharPtr) tvnp->data.ptrvalue, "Project:") == 0)
            {
                user_obj.Reset(new ncbi::objects::CUser_object);
                ncbi::objects::CObject_id& id = user_obj->SetType();

                id.SetStr("GenomeProjectsDB");
            }
            continue;
        }

        if (user_obj.Empty())
            continue;

        str = (CharPtr) tvnp->data.ptrvalue;
        if(str == NULL || *str == '\0')
            continue;

        if(*str != '0')
            while(*str >= '0' && *str <= '9')
                str++;
        if(*str != '\0')
        {
            ErrPostEx(SEV_ERROR, ERR_FORMAT_IncorrectDBLINK,
                      "Skipping invalid \"Project:\" value on the DBLINK line: \"%s\".",
                      tvnp->data.ptrvalue);
            continue;
        }

        user_field.Reset(new ncbi::objects::CUser_field);

        user_field->SetLabel().SetStr("ProjectID");
        user_field->SetData().SetInt(atoi((CharPtr)tvnp->data.ptrvalue));
        user_obj->SetData().push_back(user_field);

        user_field.Reset(new ncbi::objects::CUser_field);
        user_field->SetLabel().SetStr("ParentID");
        user_field->SetData().SetInt(0);

        user_obj->SetData().push_back(user_field);
    }

    if (user_obj.NotEmpty() && !user_obj->IsSetData())
    {
        user_obj.Reset();
    }

    if (user_obj.NotEmpty())
    {
        ncbi::CRef<ncbi::objects::CSeqdesc> descr(new ncbi::objects::CSeqdesc);
        descr->SetUser(*user_obj);
        descrs.push_back(descr);
    }

    user_obj.Reset();
    user_field.Reset();

    bool inpr = false;
    for (tvnp = vnp; tvnp != NULL; tvnp = tvnp->next)
    {
        if(StringChr((CharPtr) tvnp->data.ptrvalue, ':') != NULL)
        {
            if(StringCmp((CharPtr) tvnp->data.ptrvalue, "Project:") == 0)
            {
                inpr = true;
                continue;
            }

            inpr = false;

            if (user_obj.Empty())
            {
                user_obj.Reset(new ncbi::objects::CUser_object);
                user_obj->SetType().SetStr("DBLink");
            }

            for(i = 0, uvnp = tvnp->next; uvnp != NULL; uvnp = uvnp->next, i++)
                if(StringChr((CharPtr) uvnp->data.ptrvalue, ':') != NULL)
                    break;

            user_field.Reset(new ncbi::objects::CUser_field);

            std::string lstr((CharPtr)tvnp->data.ptrvalue);
            lstr = lstr.substr(0, lstr.size() - 1);
            user_field->SetLabel().SetStr(lstr);
            user_field->SetNum(i);
            user_field->SetData().SetStrs();

            user_obj->SetData().push_back(user_field);

            i = 0;
        }
        else if (!inpr && user_obj.NotEmpty())
        {
            user_field->SetData().SetStrs().push_back((CharPtr)tvnp->data.ptrvalue);
        }
    }

    ValNodeFreeData(vnp);

    if (user_obj.NotEmpty())
    {
        ncbi::CRef<ncbi::objects::CSeqdesc> descr(new ncbi::objects::CSeqdesc);
        descr->SetUser(*user_obj);
        descrs.push_back(descr);

        dbuop = user_obj;
    }
}

/**********************************************************/
Uint1 fta_check_con_for_wgs(ncbi::objects::CBioseq& bioseq)
{
    if (bioseq.GetInst().GetRepr() != ncbi::objects::CSeq_inst::eRepr_delta || !bioseq.GetInst().IsSetExt() || !bioseq.GetInst().GetExt().IsDelta())
        return ncbi::objects::CMolInfo::eTech_unknown;

    bool good = false;
    bool finished = true;

    ITERATE(ncbi::objects::CDelta_ext::Tdata, delta, bioseq.GetInst().GetExt().GetDelta().Get())
    {
        if (!(*delta)->IsLoc())
            continue;

        const ncbi::objects::CSeq_loc& locs = (*delta)->GetLoc();
        ncbi::objects::CSeq_loc_CI ci(locs);

        for (; ci; ++ci)
        {
            const ncbi::objects::CSeq_id* id = nullptr;

            ncbi::CConstRef<ncbi::objects::CSeq_loc> loc = ci.GetRangeAsSeq_loc();
            if (loc->IsEmpty() || loc->IsWhole() || loc->IsInt() || loc->IsPnt() || loc->IsPacked_pnt())
                id = &ci.GetSeq_id();
            else
                continue;

            if (id == nullptr)
                break;

            if (!id->IsGenbank() && !id->IsEmbl() &&
               !id->IsOther() && !id->IsDdbj() &&
               !id->IsTpg() && !id->IsTpe() && !id->IsTpd())
                break;

            const ncbi::objects::CTextseq_id* text_id = id->GetTextseq_Id();
            if (text_id == nullptr || !text_id->IsSetAccession() ||
               text_id->GetAccession().empty() ||
               fta_if_wgs_acc(text_id->GetAccession().c_str()) != 1)
                break;
            good = true;
        }

        if (ci)
        {
            finished = false;
            break;
        }
    }

    if (good && finished)
        return ncbi::objects::CMolInfo::eTech_wgs;

    return ncbi::objects::CMolInfo::eTech_unknown;
}

/**********************************************************/
static void fta_fix_seq_id(ncbi::objects::CSeq_loc& loc, ncbi::objects::CSeq_id& id, IndexblkPtr ibp,
                           CharPtr location, CharPtr name, SeqLocIdsPtr slip,
                           bool iscon, Int2 source)
{
    Uint1        accowner;
    Int4         i;
    Char         ch;

    if (ibp == NULL)
        return;

    if(name == NULL && id.IsGeneral())
    {
        const ncbi::objects::CDbtag& tag = id.GetGeneral();
        if (tag.GetDb() == "SeqLit" || tag.GetDb() == "UnkSeqLit")
            return;
    }

    if (!id.IsGenbank() && !id.IsEmbl() && !id.IsPir() &&
        !id.IsSwissprot() && !id.IsOther() && !id.IsDdbj() && !id.IsPrf() &&
        !id.IsTpg() && !id.IsTpe() && !id.IsTpd())
    {
        if(StringLen(location) > 50)
        {
            ch = location[50];
            location[50] = '\0';
        }
        else
            ch = '\0';

        if(name == NULL)
            ErrPostEx(SEV_REJECT, ERR_LOCATION_SeqIdProblem,
                      "Empty or unsupported Seq-id found in CONTIG/CO line at location: \"%s\". Entry skipped.",
                      location);
        else
            ErrPostEx(SEV_REJECT, ERR_LOCATION_SeqIdProblem,
                      "Empty or unsupported Seq-id found in feature \"%s\" at location \"%s\". Entry skipped.",
                      name, location);
        if(ch != '\0')
            location[50] = ch;
        ibp->drop = 1;
        return;
    }

    const ncbi::objects::CTextseq_id* text_id = id.GetTextseq_Id();
    if (text_id == NULL || !text_id->IsSetAccession())
    {
        if(StringLen(location) > 50)
        {
            ch = location[50];
            location[50] = '\0';
        }
        else
            ch = '\0';
        if(name == NULL)
            ErrPostEx(SEV_REJECT, ERR_LOCATION_SeqIdProblem,
                      "Empty Seq-id found in CONTIG/CO line at location: \"%s\". Entry skipped.",
                      location);
        else
            ErrPostEx(SEV_REJECT, ERR_LOCATION_SeqIdProblem,
                      "Empty Seq-id found in feature \"%s\" at location \"%s\". Entry skipped.",
                      name, location);
        if(ch != '\0')
            location[50] = ch;
        ibp->drop = 1;
        return;
    }

    const Char* accession = text_id->GetAccession().c_str();
    if(iscon)
    {
        i = IsNewAccessFormat(accession);
        if(i == 3)
        {
            if(slip->wgscont == NULL)
                slip->wgscont = accession;
            else if(slip->wgsacc == NULL &&
                    StringNCmp(slip->wgscont, accession, 4) != 0)
                    slip->wgsacc = accession;
        }
        else if(i == 7)
        {
            if(slip->wgsscaf == NULL)
                slip->wgsscaf = accession;
            else if(slip->wgsacc == NULL &&
                    StringNCmp(slip->wgsscaf, accession, 4) != 0)
                    slip->wgsacc = accession;
        }
    }

    accowner = GetNucAccOwner(accession, NULL, ibp->is_tpa);
    if(accowner == 0)
        accowner = GetProtAccOwner(accession);

    if (accowner != 0)
    {
        if (accowner != id.Which())
        {
            ncbi::CRef<ncbi::objects::CTextseq_id> new_text_id(new ncbi::objects::CTextseq_id);
            new_text_id->Assign(*text_id);
            SetTextId(accowner, id, *new_text_id);
        }
    }

    else if(source == ParFlat_FLYBASE)
    {
        std::string acc(accession);
        id.SetGeneral().SetDb("FlyBase");
        id.SetGeneral().SetTag().SetStr(acc);
    }
    else
    {
        if(StringLen(location) > 50)
        {
            ch = location[50];
            location[50] = '\0';
        }
        else
            ch = '\0';
        if(name == NULL)
            ErrPostEx(SEV_REJECT, ERR_LOCATION_SeqIdProblem,
                      "Invalid accession found in CONTIG/CO line at location: \"%s\". Entry skipped.",
                      location);
        else
            ErrPostEx(SEV_REJECT, ERR_LOCATION_SeqIdProblem,
                      "Invalid accession found in feature \"%s\" at location \"%s\". Entry skipped.",
                      name, location);
        if(ch != '\0')
            location[50] = ch;
        ibp->drop = 1;
        return;
    }

    slip->total++;

    if (id.IsGenbank())
    {
        if(source != ParFlat_NCBI && source != ParFlat_ALL &&
           source != ParFlat_LANL && slip->badslp == nullptr)
            slip->badslp = &loc;
        slip->genbank = 1;
    }
    else if(id.IsEmbl())
    {
        if(source != ParFlat_EMBL && source != ParFlat_ALL &&
           slip->badslp == nullptr)
           slip->badslp = &loc;
        slip->embl = 1;
    }
    else if(id.IsPir())
    {
        if(source != ParFlat_PIR && source != ParFlat_ALL &&
           slip->badslp == nullptr)
           slip->badslp = &loc;
        slip->pir = 1;
    }
    else if(id.IsSwissprot())
    {
        if(source != ParFlat_SPROT && source != ParFlat_ALL &&
           slip->badslp == nullptr)
           slip->badslp = &loc;
        slip->swissprot = 1;
    }
    else if(id.IsOther())
    {
        if(source != ParFlat_REFSEQ && source != ParFlat_ALL &&
           slip->badslp == nullptr)
           slip->badslp = &loc;
        slip->other = 1;
    }
    else if(id.IsDdbj())
    {
        if(source != ParFlat_DDBJ && source != ParFlat_ALL &&
           slip->badslp == nullptr)
           slip->badslp = &loc;
        slip->ddbj = 1;
    }
    else if(id.IsPrf())
    {
        if(source != ParFlat_PRF && source != ParFlat_ALL &&
           slip->badslp == nullptr)
           slip->badslp = &loc;
        slip->prf = 1;
    }
    else if(id.IsTpg())
    {
        if(source != ParFlat_NCBI && source != ParFlat_ALL &&
           source != ParFlat_LANL && slip->badslp == nullptr)
           slip->badslp = &loc;
        slip->tpg = 1;
    }
    else if (id.IsTpe())
    {
        if(source != ParFlat_EMBL && source != ParFlat_ALL &&
           slip->badslp == nullptr)
           slip->badslp = &loc;
        slip->tpe = 1;
    }
    else if (id.IsTpd())
    {
        if(source != ParFlat_DDBJ && source != ParFlat_ALL &&
           slip->badslp == nullptr)
           slip->badslp = &loc;
        slip->tpd = 1;
    }
}

/**********************************************************/
static void fta_do_fix_seq_loc_id(TSeqLocList& locs, IndexblkPtr ibp,
                                  CharPtr location, CharPtr name,
                                  SeqLocIdsPtr slip, bool iscon, Int2 source)
{
    NON_CONST_ITERATE(TSeqLocList, loc, locs)
    {
        if ((*loc)->IsEmpty())
        {
            fta_fix_seq_id(*(*loc), (*loc)->SetEmpty(), ibp,
                           location, name, slip, iscon, source);
        }
        else if ((*loc)->IsWhole())
        {
            fta_fix_seq_id(*(*loc), (*loc)->SetWhole(), ibp,
                           location, name, slip, iscon, source);
        }
        else if ((*loc)->IsInt())
        {
            fta_fix_seq_id(*(*loc), (*loc)->SetInt().SetId(), ibp, location, name, slip, iscon, source);
        }
        else if ((*loc)->IsPnt())
        {
            fta_fix_seq_id(*(*loc), (*loc)->SetPnt().SetId(), ibp, location, name, slip, iscon, source);
            if (iscon && !(*loc)->GetPnt().IsSetFuzz())
            {
                int point = (*loc)->GetPnt().GetPoint();
                ncbi::CRef<ncbi::objects::CSeq_interval> interval(new ncbi::objects::CSeq_interval);
                interval->SetFrom(point);
                interval->SetTo(point);

                if ((*loc)->GetPnt().IsSetStrand())
                    interval->SetStrand((*loc)->GetPnt().GetStrand());

                interval->SetId((*loc)->SetPnt().SetId());
                (*loc)->SetInt(*interval);
            }
        }
        else if ((*loc)->IsPacked_int())
        {
            NON_CONST_ITERATE(ncbi::objects::CPacked_seqint::Tdata, interval, (*loc)->SetPacked_int().Set())
            {
                fta_fix_seq_id(*(*loc), (*interval)->SetId(), ibp, location, name, slip, iscon, source);
            }
        }
        else if ((*loc)->IsPacked_pnt())
        {
            fta_fix_seq_id(*(*loc), (*loc)->SetPacked_pnt().SetId(), ibp, location, name, slip, iscon, source);
        }
        else if ((*loc)->IsMix())
        {
            fta_do_fix_seq_loc_id((*loc)->SetMix().Set(), ibp, location, name, slip, iscon, source);
        }
        else if ((*loc)->IsEquiv())
        {
            fta_do_fix_seq_loc_id((*loc)->SetEquiv().Set(), ibp,
                                  location, name, slip, iscon, source);
        }
    }
}

/**********************************************************/
Int4 fta_fix_seq_loc_id(TSeqLocList& locs, ParserPtr pp, CharPtr location,
                        CharPtr name, bool iscon)
{
    SeqLocIds   sli;
    const Char  *p = NULL;
    ErrSev      sev;
    IndexblkPtr ibp;
    Char        ch;
    Int4        tpa;
    Int4        non_tpa;
    Int4        i = 0;

    ibp = pp->entrylist[pp->curindx];

    MemSet(&sli, 0, sizeof(SeqLocIds));
    fta_do_fix_seq_loc_id(locs, ibp, location, name, &sli, iscon, pp->source);

    tpa = sli.tpg + sli.tpe + sli.tpd;
    non_tpa = sli.genbank + sli.embl + sli.pir + sli.swissprot + sli.other +
              sli.ddbj + sli.prf;

    if(iscon && sli.wgsacc == NULL && sli.wgscont != NULL &&
       sli.wgsscaf != NULL && StringNCmp(sli.wgscont, sli.wgsscaf, 4) != 0)
        sli.wgsacc = sli.wgsscaf;

    ch = '\0';
    if((tpa > 0 && non_tpa > 0) || tpa > 1 || non_tpa > 1 ||
       (iscon && sli.wgscont != NULL && sli.wgsscaf != NULL))
    {
        if(StringLen(location) > 50)
        {
            ch = location[50];
            location[50] = '\0';
        }
    }

    if(tpa > 0 && non_tpa > 0)
    {
        if(name == NULL)
            ErrPostEx(SEV_REJECT, ERR_LOCATION_TpaAndNonTpa,
                      "The CONTIG/CO line with location \"%s\" refers to intervals on both primary and third-party sequence records. Entry skipped.",
                      location);
        else
            ErrPostEx(SEV_REJECT, ERR_LOCATION_TpaAndNonTpa,
                      "The \"%s\" feature at \"%s\" refers to intervals on both primary and third-party sequence records. Entry skipped.",
                      name, location);
        ibp->drop = 1;
    }

    if(tpa > 1 || non_tpa > 1)
    {
        if (!pp->allow_crossdb_featloc)
        {
            sev = SEV_REJECT;
            p = (CharPtr) "Entry skipped.";
            ibp->drop = 1;
        }
        else
        {
            sev = SEV_WARNING;
            p = (CharPtr) "";
        }
        if(name == NULL)
        {
            std::string label;
            if (sli.badslp != nullptr)
                sli.badslp->GetLabel(&label);

            ErrPostEx(sev, ERR_LOCATION_CrossDatabaseFeatLoc,
                      "The CONTIG/CO line refers to intervals on records from two or more INSDC databases. This is not allowed without review and approval : \"%s\".%s",
                      label.empty() ? location : label.c_str(), p);
        }
        else
            ErrPostEx(sev, ERR_LOCATION_CrossDatabaseFeatLoc,
                      "The \"%s\" feature at \"%s\" refers to intervals on records from two or more INSDC databases. This is not allowed without review and approval.%s",
                      name, location, p);
    }

    if(iscon)
    {
        if(sli.wgscont != NULL && sli.wgsscaf != NULL)
            ErrPostEx(SEV_ERROR, ERR_LOCATION_ContigAndScaffold,
                      "The CONTIG/CO line with location \"%s\" refers to intervals on both WGS contig and WGS scaffold records.",
                      location);

        if(sli.wgsacc != NULL)
        {
            if(sli.wgscont != NULL &&
               StringNCmp(sli.wgscont, sli.wgsacc, 4) != 0)
                p = sli.wgscont;
            else if(sli.wgsscaf != NULL &&
                    StringNCmp(sli.wgsscaf, sli.wgsacc, 4) != 0)
                p = sli.wgsscaf;

            if(p != NULL)
            {
                Char msga[5],
                     msgb[5];

                StringNCpy(msga, sli.wgsacc, 4);
                StringNCpy(msgb, p, 4);
                msga[4] = msgb[4] = 0;

                ErrPostEx(SEV_WARNING, ERR_SEQUENCE_MultipleWGSProjects,
                          "This CON/scaffold record is assembled from the contigs of multiple WGS projects. First pair of WGS project codes is \"%s\" and \"%s\".",
                          msgb, msga);
            }
        }

        i = IsNewAccessFormat(ibp->acnum);
        if(i == 3 || i == 7)
        {
            p = NULL;
            if(sli.wgscont != NULL &&
               StringNCmp(sli.wgscont, ibp->acnum, 4) != 0)
                p = sli.wgscont;
            else if(sli.wgsscaf != NULL &&
                    StringNCmp(sli.wgsscaf, ibp->acnum, 4) != 0)
                p = sli.wgsscaf;
            else if(sli.wgsacc != NULL &&
                    StringNCmp(sli.wgsacc, ibp->acnum, 4) != 0)
                p = sli.wgsscaf;

            if(p != NULL)
            {
                Char msg[5];
                StringNCpy(msg, p, 4);
                msg[4] = 0;

                ErrPostEx(SEV_WARNING, ERR_ACCESSION_WGSPrefixMismatch,
                          "This WGS CON/scaffold record is assembled from the contigs of different WGS projects. First differing WGS project code is \"%s\".",
                          msg);
            }
        }
    }

    if(ch != '\0')
        location[50] = ch;

    if(sli.wgscont != NULL)
        sli.wgscont = NULL;
    if(sli.wgsscaf != NULL)
        sli.wgsscaf = NULL;
    if(sli.wgsacc != NULL)
        sli.wgsacc = NULL;

    return(sli.total);
}

/**********************************************************/
static ValNodePtr fta_vnp_structured_comment(CharPtr buf)
{
    ValNodePtr res;
    ValNodePtr vnp;
    CharPtr    start;
    CharPtr    p;
    CharPtr    q;
    CharPtr    r;
    bool       bad;

    if(buf == NULL || *buf == '\0')
        return(NULL);

    for(p = buf; *p != '\0'; p++)
    {
        if(*p != '~')
            continue;

        for(p++; *p == ' ' || *p == '~'; p++)
            *p = ' ';
        p--;
    }

    bad = false;
    res = ValNodeNew(NULL);
    vnp = res;
    for(start = buf, q = start;;)
    {
        p = StringStr(start, "::");
        if(p == NULL)
        {
            if(start == buf)
                bad = true;
            break;
        }

        q = StringStr(p + 2, "::");
        if(q == NULL)
        {
            vnp->next = ValNodeNew(NULL);
            vnp = vnp->next;
            vnp->data.ptrvalue = StringSave(start);
            for(r = (CharPtr) vnp->data.ptrvalue; *r != '\0'; r++)
                if(*r == '~')
                    *r = ' ';
            ShrinkSpaces((CharPtr) vnp->data.ptrvalue);
            break;
        }

        *q = '\0';
        r = StringRChr(p + 2, '~');
        *q = ':';
        if(r == NULL)
        {
            bad = true;
            break;
        }

        *r = '\0';
        vnp->next = ValNodeNew(NULL);
        vnp = vnp->next;
        vnp->data.ptrvalue = StringSave(start);
        *r = '~';
        for(p = (CharPtr) vnp->data.ptrvalue; *p != '\0'; p++)
            if(*p == '~')
                *p = ' ';
        ShrinkSpaces((CharPtr) vnp->data.ptrvalue);

        start = r;
    }

    vnp = res->next;
    res->next = NULL;
    ValNodeFree(res);

    if(!bad)
        return(vnp);

    ValNodeFreeData(vnp);
    return(NULL);
}

/**********************************************************/
static ncbi::CRef<ncbi::objects::CUser_object> fta_build_structured_comment(CharPtr tag, CharPtr buf)
{
    ValNodePtr    vnp;
    ValNodePtr    tvnp;

    CharPtr       p;
    CharPtr       q;

    ncbi::CRef<ncbi::objects::CUser_object> obj;

    if (tag == NULL || *tag == '\0' || buf == NULL || *buf == '\0')
        return obj;

    vnp = fta_vnp_structured_comment(buf);
    if(vnp == NULL)
        return obj;

    obj.Reset((new ncbi::objects::CUser_object));

    ncbi::objects::CObject_id& id = obj->SetType();
    id.SetStr("StructuredComment");

    ncbi::CRef<ncbi::objects::CUser_field> field(new ncbi::objects::CUser_field);
    field->SetLabel().SetStr("StructuredCommentPrefix");

    field->SetData().SetStr() = tag;
    field->SetData().SetStr() += "-START##";

    obj->SetData().push_back(field);

    for(tvnp = vnp; tvnp != NULL; tvnp = tvnp->next)
    {
        p = (CharPtr) tvnp->data.ptrvalue;
        if(p == NULL || *p == '\0')
            continue;

        q = StringStr(p, "::");
        if(q == NULL)
            continue;

        if(q > p && *(q - 1) == ' ')
            q--;

        for(*q++ = '\0'; *q == ' ' || *q == ':';)
            q++;

        if(*p == '\0' || *q == '\0')
            continue;

        field.Reset(new ncbi::objects::CUser_field);
        field->SetLabel().SetStr(p);
        field->SetData().SetStr(q);

        obj->SetData().push_back(field);
    }

    if (obj->GetData().size() < 2)
    {
        obj.Reset();
        return obj;
    }

    field.Reset(new ncbi::objects::CUser_field);
    field->SetLabel().SetStr("StructuredCommentSuffix");
    field->SetData().SetStr() = tag;
    field->SetData().SetStr() += "-END##";

    obj->SetData().push_back(field);

    ValNodeFreeData(vnp);

    return obj;
}

/**********************************************************/
void fta_parse_structured_comment(CharPtr str, bool& bad, TUserObjVector& objs)
{
    ValNodePtr    tagvnp;
    ValNodePtr    vnp;

    CharPtr       start;
    CharPtr       tag = NULL;
    CharPtr       buf;
    CharPtr       p;
    CharPtr       q;
    CharPtr       r;

    if(str == NULL || *str == '\0')
        return;

    tagvnp = NULL;
    for(p = str;;)
    {
        p = StringStr(p, "-START##");
        if(p == NULL)
            break;
        for(q = p;; q--)
            if(*q == '~' || (*q == '#' && q > str && *--q == '#') || q == str)
                break;
        if(q[0] != '#' || q[1] != '#')
        {
            p += 8;
            continue;
        }

        start = q;

        *p = '\0';
        tag = StringSave(q);
        *p = '-';

        for(q = p;;)
        {
            q = StringStr(q, tag);
            if(q == NULL)
            {
                bad = true;
                break;
            }
            size_t i = StringLen(tag);
            if(StringNCmp(q + i, "-END##", 6) != 0)
            {
                q += (i + 6);
                continue;
            }
            r = StringStr(p + 8, "-START##");
            if(r != NULL && r < q)
            {
                bad = true;
                break;
            }
            break;
        }

        if (bad)
            break;

        if(tagvnp == NULL)
        {
            tagvnp = ValNodeNew(NULL);
            tagvnp->data.ptrvalue = StringSave(tag);
            tagvnp->next = NULL;
        }
        else
        {
            for(vnp = tagvnp; vnp != NULL; vnp = vnp->next)
            {
                r = (CharPtr) vnp->data.ptrvalue;
                if(StringCmp(r + 2, tag + 2) == 0)
                {
                    if(*r != ' ')
                    {
                        ErrPostEx(SEV_ERROR, ERR_COMMENT_SameStructuredCommentTags,
                                  "More than one structured comment with the same tag \"%s\" found.",
                                  tag + 2);
                        *r = ' ';
                    }
                    break;
                }
                if(vnp->next == NULL)
                {
                    vnp->next = ValNodeNew(NULL);
                    vnp->next->data.ptrvalue = StringSave(tag);
                    vnp->next->next = NULL;
                    break;
                }
            }
        }

        if(StringCmp(tag, "##Metadata") == 0)
        {
            MemFree(tag);
            p += 8;
            continue;
        }

        *q = '\0';
        if(StringStr(p + 8, "::") == NULL)
        {
            ErrPostEx(SEV_ERROR, ERR_COMMENT_StructuredCommentLacksDelim,
                      "The structured comment in this record lacks the expected double-colon '::' delimiter between fields and values.");
            MemFree(tag);
            p += 8;
            *q = '#';
            continue;
        }

        buf = StringSave(p + 8);
        *q = '#';

        ncbi::CRef<ncbi::objects::CUser_object> cur = fta_build_structured_comment(tag, buf);
        MemFree(buf);

        if (cur.Empty())
        {
            bad = true;
            break;
        }

        objs.push_back(cur);

        fta_StringCpy(start, q + StringLen(tag) + 6);
        MemFree(tag);
        p = start;
    }

    if(bad)
    {
        ErrPostEx(SEV_REJECT, ERR_COMMENT_InvalidStructuredComment,
                  "Incorrectly formatted structured comment with tag \"%s\" encountered. Entry dropped.",
                  tag + 2);
        MemFree(tag);
    }

    if(tagvnp != NULL)
        ValNodeFreeData(tagvnp);
}

/**********************************************************/
CharPtr GetQSFromFile(FILE PNTR fd, IndexblkPtr ibp)
{
    CharPtr ret;
    Char    buf[1024];

    if(fd == NULL || ibp->qslength < 1)
        return(NULL);

    ret = (CharPtr) MemNew(ibp->qslength + 10);
    ret[0] = '\0';
    fseek(fd, static_cast<long>(ibp->qsoffset), 0);
    while(fgets(buf, 1023, fd) != NULL)
    {
        if(buf[0] == '>' && ret[0] != '\0')
            break;
        StringCat(ret, buf);
    }
    return(ret);
}

/**********************************************************/
void fta_remove_cleanup_user_object(ncbi::objects::CSeq_entry& seq_entry)
{
    TSeqdescList* descrs = nullptr;
    if (seq_entry.IsSeq())
    {
        if (seq_entry.GetSeq().IsSetDescr())
            descrs = &seq_entry.SetSeq().SetDescr().Set();
    }
    else if (seq_entry.IsSet())
    {
        if (seq_entry.GetSet().IsSetDescr())
            descrs = &seq_entry.SetSet().SetDescr().Set();
    }

    if (descrs == nullptr)
        return;

    for (TSeqdescList::iterator descr = descrs->begin(); descr != descrs->end(); )
    {
        if (!(*descr)->IsUser())
        {
            ++descr;
            continue;
        }

        const ncbi::objects::CUser_object& user_obj = (*descr)->GetUser();
        if (!user_obj.IsSetType() || !user_obj.GetType().IsStr() ||
            user_obj.GetType().GetStr() != "NcbiCleanup")
        {
            ++descr;
            continue;
        }

        descr = descrs->erase(descr);
        break;
    }
}

/**********************************************************/
void fta_tsa_tls_comment_dblink_check(const ncbi::objects::CBioseq& bioseq,
                                      bool is_tsa)
{
    bool got_comment = false;
    bool got_dblink = false;

    ITERATE(TSeqdescList, descr, bioseq.GetDescr().Get())
    {
        if (!(*descr)->IsUser())
            continue;

        const ncbi::objects::CUser_object& user_obj = (*descr)->GetUser();
        if (!user_obj.IsSetType() || !user_obj.GetType().IsStr())
            continue;

        const std::string& user_type_str = user_obj.GetType().GetStr();

        if (user_type_str == "StructuredComment")
            got_comment = true;
        else if (user_type_str == "GenomeProjectsDB")
            got_dblink = true;
        else if (user_type_str == "DBLink")
        {
            ITERATE(ncbi::objects::CUser_object::TData, field, user_obj.GetData())
            {
                if (!(*field)->IsSetLabel() || !(*field)->GetLabel().IsStr() ||
                    (*field)->GetLabel().GetStr() != "BioProject")
                    continue;
                got_dblink = true;
                break;
            }
        }
    }

    if(!is_tsa)
    {
        if(!got_comment)
            ErrPostEx(SEV_WARNING, ERR_ENTRY_TLSLacksStructuredComment,
                      "This TLS record lacks an expected structured comment.");
        if(!got_dblink)
            ErrPostEx(SEV_WARNING, ERR_ENTRY_TLSLacksBioProjectLink,
                      "This TLS record lacks an expected BioProject or Project link.");
    }
    else
    {
        if(!got_comment)
            ErrPostEx(SEV_WARNING, ERR_ENTRY_TSALacksStructuredComment,
                      "This TSA record lacks an expected structured comment.");
        if(!got_dblink)
            ErrPostEx(SEV_WARNING, ERR_ENTRY_TSALacksBioProjectLink,
                      "This TSA record lacks an expected BioProject or Project link.");
    }
}

/**********************************************************/
void fta_set_molinfo_completeness(ncbi::objects::CBioseq& bioseq, IndexblkPtr ibp)
{
    if (bioseq.GetInst().GetTopology() != 2 || (ibp != NULL && ibp->gaps != NULL))
        return;

    ncbi::objects::CMolInfo* mol_info = nullptr;
    NON_CONST_ITERATE(TSeqdescList, descr, bioseq.SetDescr().Set())
    {
        if ((*descr)->IsMolinfo())
        {
            mol_info = &(*descr)->SetMolinfo();
            break;
        }
    }

    if (mol_info != nullptr)
    {
        mol_info->SetCompleteness(1);
    }
    else
    {
        ncbi::CRef<ncbi::objects::CSeqdesc> descr(new ncbi::objects::CSeqdesc);
        ncbi::objects::CMolInfo& mol = descr->SetMolinfo();
        mol.SetCompleteness(1);

        bioseq.SetDescr().Set().push_back(descr);
    }
}

/**********************************************************/
void fta_create_far_fetch_policy_user_object(ncbi::objects::CBioseq& bsp, Int4 num)
{
    if (num < 1000)
        return;

    ErrPostEx(SEV_INFO, ERR_SEQUENCE_HasManyComponents,
              "An OnlyNearFeatures FeatureFetchPolicy User-object has been added to this record because it is constructed from %d components, which exceeds the threshold of 999 for User-object creation.",
              num);

    ncbi::CRef<ncbi::objects::CSeqdesc> descr(new ncbi::objects::CSeqdesc);
    descr->SetUser().SetType().SetStr("FeatureFetchPolicy");

    ncbi::CRef<ncbi::objects::CUser_field> field(new ncbi::objects::CUser_field);

    field->SetLabel().SetStr("Policy");
    field->SetData().SetStr("OnlyNearFeatures");

    descr->SetUser().SetData().push_back(field);

    bsp.SetDescr().Set().push_back(descr);
}

/**********************************************************/
void StripECO(CharPtr str)
{
    CharPtr p;
    CharPtr q;

    if(str == NULL || *str == '\0')
        return;

    p = StringStr(str, "{ECO:");
    if(p == NULL)
        return;

    for(;;)
    {
        q = StringChr(p + 1, '}');
        if(q == NULL)
            break;
        if(p > str && *(p - 1) == ' ')
            p--;
        if(p > str)
            if((*(p - 1) == '.' && q[1] == '.') ||
               (*(p - 1) == ';' && q[1] == ';'))
                p--;
        fta_StringCpy(p, q + 1);
        p = StringStr(p, "{ECO:");
        if(p == NULL)
            break;
    }
}

/**********************************************************/
bool fta_dblink_has_sra(const ncbi::CRef<ncbi::objects::CUser_object>& uop)
{
    if (uop.Empty() || !uop->IsSetData() || !uop->IsSetType() ||
        !uop->GetType().IsStr() || uop->GetType().GetStr() != "DBLink")
        return false;

    bool got = false;

    ITERATE(ncbi::objects::CUser_object::TData, field, uop->GetData())
    {
        if (!(*field)->IsSetData() || !(*field)->GetData().IsStrs() || !(*field)->IsSetNum() || (*field)->GetNum() < 1 ||
            !(*field)->IsSetLabel() || !(*field)->GetLabel().IsStr() || (*field)->GetLabel().GetStr() != "Sequence Read Archive")
            continue;

        ITERATE(ncbi::objects::CUser_field::C_Data::TStrs, str, (*field)->GetData().GetStrs())
        {
            if (str->size() > 2 &&
                ((*str)[0] == 'D' || (*str)[0] == 'E' || (*str)[0] == 'S') && (*str)[1] == 'R' &&
                ((*str)[2] == 'R' || (*str)[2] == 'X' || (*str)[2] == 'Z'))
            {
                got = true;
                break;
            }
        }
        if(got)
            break;
    }
    return(got);
}
