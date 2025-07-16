/* $Id$
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
 * File Name: add.cpp
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 *      Additional parser functions.
 *
 */

#include <ncbi_pch.hpp>

#include "ftacpp.hpp"
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
#include <objects/seq/seq_id_handle.hpp>

#include "index.h"
#include "genbank.h" /* for ParFlat_FEATURES */
#include "embl.h"    /* for ParFlat_FH */

#include <objtools/flatfile/flatdefn.h>
#include "ftanet.h"

#include "ftaerr.hpp"
#include "indx_blk.h"
#include "asci_blk.h"
#include "utilfun.h"
#include "add.h"

#ifdef THIS_FILE
#  undef THIS_FILE
#endif
#define THIS_FILE "add.cpp"

#define HTG_GAP   100
#define SHORT_GAP 20

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

struct SeqLocIds {
    CSeq_loc*   badslp    = nullptr;
    const Char* wgsacc    = nullptr;
    const Char* wgscont   = nullptr;
    const Char* wgsscaf   = nullptr;
    Int4        genbank   = 0;
    Int4        embl      = 0;
    Int4        pir       = 0;
    Int4        swissprot = 0;
    Int4        other     = 0;
    Int4        ddbj      = 0;
    Int4        prf       = 0;
    Int4        tpg       = 0;
    Int4        tpe       = 0;
    Int4        tpd       = 0;
    Int4        total     = 0;
};
using SeqLocIdsPtr = SeqLocIds*;

struct FTATpaBlock {
    Int4              from1     = 0;
    Int4              to1       = 0;
    char*             accession = nullptr;
    Int4              version   = 0;
    Int4              from2     = 0;
    Int4              to2       = 0;
    ENa_strand        strand    = eNa_strand_unknown;
    CSeq_id::E_Choice sicho     = CSeq_id::e_not_set;
    FTATpaBlock*      next      = nullptr;
};
using FTATpaBlockPtr = FTATpaBlock*;

struct FTATpaSpan {
    Int4        from = 0;
    Int4        to   = 0;
    FTATpaSpan* next = nullptr;
};
using FTATpaSpanPtr = FTATpaSpan*;

/**********************************************************/
static void fta_tpa_block_free(FTATpaBlockPtr ftbp)
{
    FTATpaBlockPtr next;

    for (; ftbp; ftbp = next) {
        next = ftbp->next;
        if (ftbp->accession)
            MemFree(ftbp->accession);
        delete ftbp;
    }
}

/**********************************************************
 *
 *   char* tata_save(str):
 *
 *      Deletes spaces from the begining and the end and
 *   returns Nlm_StringSave.
 *
 **********************************************************/
string tata_save(string_view t)
{
    if (t.empty())
        return {};
    string str(t);

    // strip from beginning
    size_t i = 0;
    for (char c : str) {
        if (isspace(c) || c == ',')
            ++i;
        else
            break;
    }
    if (i > 0)
        str.erase(0, i);

    // strip from beginning of each line
    for (i = 0; i < str.length(); ++i) {
        if (str[i] != '\n')
            continue;
        size_t j = 0;
        for (size_t k = i + 1; k < str.length() && isspace(str[k]); ++k)
            ++j;
        str[i] = ' ';
        if (j > 0)
            str.erase(i + 1, j);
    }

    // strip from end
    while (! str.empty()) {
        char c = str.back();
        if (c == ' ' || c == ';' || c == ',' || c == '\"' || c == '\t')
            str.pop_back();
        else
            break;
    }

    return str;
}

/**********************************************************/
bool no_date(Parser::EFormat format, const TSeqdescList& descrs)
{
    bool no_create = true;
    bool no_update = true;

    for (const auto& desc : descrs) {
        if (desc->IsCreate_date())
            no_create = false;
        else if (desc->IsUpdate_date())
            no_update = false;

        if (no_create == false && no_update == false)
            break;
    }

    if (format == Parser::EFormat::GenBank)
        return (no_update);

    return (no_create || no_update);
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
bool no_reference(const CBioseq& bioseq)
{
    for (const auto& desc : bioseq.GetDescr().Get()) {
        if (desc->IsPub())
            return false;
    }

    for (const auto& annot : bioseq.GetAnnot()) {
        if (! annot->IsFtable())
            continue;

        for (const auto& feat : annot->GetData().GetFtable()) {
            if (feat->IsSetData() && feat->GetData().IsPub())
                return false;
        }

        for (const auto& feat : annot->GetData().GetFtable()) {
            if (! feat->IsSetData() || ! feat->GetData().IsImp())
                continue;

            const CImp_feat& imp = feat->GetData().GetImp();
            if (imp.GetKey() == "Site-ref") {
                FtaErrPost(SEV_ERROR, ERR_REFERENCE_Illegalreference, "The entry has only 'sites' references");
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
bool check_cds(const DataBlk& entry, Parser::EFormat format)
{
    const char* str;
    Int2        type;

    if (format == Parser::EFormat::EMBL) {
        type = ParFlat_FH;
        str  = "\nFT   CDS  ";
    } else if (format == Parser::EFormat::GenBank) {
        type = ParFlat_FEATURES;
        str  = "\n     CDS  ";
    } else
        return false;

    const auto& chain = TrackNodes(entry);
    auto        temp  = chain.cbegin();
    for (; temp != chain.cend(); ++temp) {
        const auto& dblk = *temp;
        if (dblk.mType != type)
            continue;

        size_t len = 0;

        const auto& subblocks = dblk.GetSubBlocks();
        for (const auto& dbp : subblocks)
            len += dbp.mBuf.len;
        if (len == 0)
            continue;

        auto dbp = subblocks.cbegin();
        char* p  = SrchTheStr(dbp->mBuf.ptr, dbp->mBuf.ptr + len, str);
        if (p)
            break;
    }
    if (temp == chain.cend())
        return false;

    return true;
}

/**********************************************************/
void err_install(const Indexblk* ibp, bool accver)
{
    FtaInstallPrefix(PREFIX_LOCUS, ibp->locusname);

    string temp = ibp->acnum;
    if (accver && ibp->vernum > 0) {
        temp += '.';
        temp += to_string(ibp->vernum);
    }
    if (temp.empty())
        temp = ibp->locusname;
    FtaInstallPrefix(PREFIX_ACCESSION, temp);
}

/**********************************************************/
static void CreateSeqGap(CSeq_literal& seq_lit, GapFeats& gfp)
{
    CSeq_gap& sgap = seq_lit.SetSeq_data().SetGap();
    sgap.SetType(gfp.asn_gap_type);

    if (! gfp.asn_linkage_evidence.empty())
        sgap.SetLinkage_evidence().swap(gfp.asn_linkage_evidence);

    sgap.SetLinkage(CSeq_gap::eLinkage_unlinked);
    if (! gfp.gap_type.empty()) {
        const string& gapType(gfp.gap_type);
        if (gapType == "unknown" || gapType == "within scaffold" || gapType == "repeat within scaffold") {
            sgap.SetLinkage(CSeq_gap::eLinkage_linked);
        }
    }
}

/**********************************************************/
void AssemblyGapsToDelta(CBioseq& bioseq, TGapFeatsList& gf, bool* drop)
{
    if (! bioseq.GetInst().IsSetExt() || ! bioseq.GetInst().GetExt().IsDelta() ||
        gf.empty())
        return;

    GapFeatsPtr                 gfp    = gf.begin();
    CDelta_ext::Tdata&          deltas = bioseq.SetInst().SetExt().SetDelta();
    CDelta_ext::Tdata::iterator delta  = deltas.begin();
    for (; delta != deltas.end(); ++delta) {
        if (gfp == gf.end())
            break;

        if (! (*delta)->IsLiteral()) /* not Seq-lit */
            continue;

        CSeq_literal& literal = (*delta)->SetLiteral();
        if (literal.GetLength() != static_cast<Uint4>(gfp->to - gfp->from + 1)) {
            FtaErrPost(SEV_REJECT, ERR_FORMAT_ContigVersusAssemblyGapMissmatch, 
                    "The lengths of the CONTIG/CO line gaps disagrees with the lengths of assembly_gap features. First assembly_gap with a mismatch is at \"{}..{}\".", 
                    gfp->from, gfp->to);
            *drop = true;
            break;
        }

        CreateSeqGap(literal, *gfp);

        ++gfp;
    }

    if (*drop || (delta == deltas.end() && gfp == gf.end()))
        return;

    if (delta == deltas.end() && gfp != gf.end()) {
        FtaErrPost(SEV_REJECT, ERR_FORMAT_ContigVersusAssemblyGapMissmatch, 
                "The number of the assembly_gap features exceeds the number of CONTIG/CO line gaps. First extra assembly_gap is at \"{}..{}\".", 
                gfp->from, gfp->to);
        *drop = true;
    } else if (delta != deltas.end() && gfp == gf.end()) {
        for (; delta != deltas.end(); ++delta) {
            if ((*delta)->IsLiteral()) /* Seq-lit */
                break;
        }

        if (delta == deltas.end())
            return;

        FtaErrPost(SEV_REJECT, ERR_FORMAT_ContigVersusAssemblyGapMissmatch, "The number of the CONTIG/CO line gaps exceeds the number of assembly_gap features.");
        *drop = true;
    }
}

/**********************************************************/
void GapsToDelta(CBioseq& bioseq, TGapFeatsList& gf, bool* drop)
{
    const Char* p;
    Int4        prevto;
    Int4        nextfrom;
    Int4        i;

    if (gf.empty() || ! bioseq.GetInst().IsSetSeq_data())
        return;

    const string& sequence = bioseq.GetInst().GetSeq_data().GetIupacna();

    if (sequence.empty() || sequence.size() != bioseq.GetLength())
        return;

    prevto = 0;
    for (auto tgfp = gf.begin(); tgfp != gf.end(); ++tgfp) {
        auto const nxt = next(tgfp);
        if (nxt != gf.end()) {
            p = sequence.c_str() + tgfp->to;
            for (i = tgfp->to + 1; i < nxt->from; p++, i++)
                if (*p != 'N')
                    break;
            if (i == nxt->from && nxt->from > tgfp->to + 1) {
                FtaErrPost(SEV_ERROR, ERR_FEATURE_AllNsBetweenGaps, 
                        "A run of all-N sequence exists between the gap features located at \"{}..{}\" and \"{}..{}\".", 
                        tgfp->from, tgfp->to, nxt->from, nxt->to);
                tgfp->rightNs      = true;
                nxt->leftNs = true;
            }
            nextfrom = nxt->from;
        } else
            nextfrom = bioseq.GetLength() + 1;

        if (tgfp->leftNs == false && tgfp->from - prevto > 10) {
            for (p = sequence.c_str() + tgfp->from - 11, i = 0; i < 10; p++, i++)
                if (*p != 'N')
                    break;
            if (i == 10) {
                FtaErrPost(SEV_WARNING, ERR_FEATURE_NsAbutGap, 
                        "A run of N's greater or equal than 10 abuts the gap feature at \"{}..{}\" : possible problem with the boundaries of the gap.", 
                        tgfp->from, tgfp->to);
            }
        }

        if (tgfp->rightNs == false && nextfrom - tgfp->to > 10) {
            for (p = sequence.c_str() + tgfp->to, i = 0; i < 10; p++, i++)
                if (*p != 'N')
                    break;
            if (i == 10) {
                FtaErrPost(SEV_WARNING, ERR_FEATURE_NsAbutGap, 
                        "A run of N's greater or equal than 10 abuts the gap feature at \"{}..{}\" : possible problem with the boundaries of the gap.", 
                        tgfp->from, tgfp->to);
            }
        }

        for (i = tgfp->from - 1, p = sequence.c_str() + i; i < tgfp->to; p++, i++)
            if (*p != 'N')
                break;
        if (i < tgfp->to) {
            FtaErrPost(SEV_REJECT, ERR_FEATURE_InvalidGapSequence, 
                    "The sequence data associated with the gap feature at \"{}..{}\" contains basepairs other than N.", 
                    tgfp->from, tgfp->to);
            *drop = true;
        }

        prevto = tgfp->to;
    }

    if (*drop)
        return;

    CDelta_ext::Tdata deltas;

    prevto = 0;
    for (GapFeatsPtr tgfp = gf.begin();;) {
        Int4 len = 0;

        CRef<CDelta_seq> delta(new CDelta_seq);
        if (tgfp->from - prevto - 1 > 0) {
            len = tgfp->from - prevto - 1;
            delta->SetLiteral().SetLength(len);
            delta->SetLiteral().SetSeq_data().SetIupacna().Set() = sequence.substr(prevto, len);

            deltas.push_back(delta);

            delta.Reset(new CDelta_seq);
        }

        len = tgfp->to - tgfp->from + 1;
        delta->SetLiteral().SetLength(len);
        if (tgfp->estimated_length == -100) {
            delta->SetLiteral().SetFuzz().SetLim();
        } else if (tgfp->estimated_length != len) {
            delta->SetLiteral().SetFuzz().SetRange().SetMin(tgfp->estimated_length);
            delta->SetLiteral().SetFuzz().SetRange().SetMax(len);
        }

        if (tgfp->assembly_gap)
            CreateSeqGap(delta->SetLiteral(), *tgfp);

        deltas.push_back(delta);

        prevto = tgfp->to;

        ++tgfp;
        if (tgfp == gf.end()) {
            if (bioseq.GetLength() - prevto > 0) {
                delta.Reset(new CDelta_seq);

                len = bioseq.GetLength() - prevto;
                delta->SetLiteral().SetLength(len);
                delta->SetLiteral().SetSeq_data().SetIupacna().Set() = sequence.substr(prevto, len);

                deltas.push_back(delta);
            }
            break;
        }
    }

    if (! deltas.empty()) {
        bioseq.SetInst().SetExt().SetDelta().Set().swap(deltas);
        bioseq.SetInst().SetRepr(CSeq_inst::eRepr_delta);
        bioseq.SetInst().ResetSeq_data();
    }
}

/**********************************************************/
void SeqToDelta(CBioseq& bioseq, Int2 tech)
{
    char* p;
    char* q;
    char* r;

    Int4 i;
    Int4 j;
    Int4 gotcha;

    if (! bioseq.GetInst().IsSetSeq_data())
        return;

    const string& sequence = bioseq.GetInst().GetSeq_data().GetIupacna();
    if (sequence.empty() || sequence.size() != bioseq.GetLength())
        return;

    vector<Char> buf(sequence.begin(), sequence.end());
    buf.push_back(0);
    p      = &buf[0];
    gotcha = 0;

    CDelta_ext::Tdata deltas;

    for (q = p; *p != '\0';) {
        if (*p != 'N') {
            p++;
            continue;
        }

        for (r = p, p++, i = 1; *p == 'N'; i++)
            p++;
        if (i < HTG_GAP) {
            if (i >= SHORT_GAP && gotcha == 0)
                gotcha = 1;
            continue;
        }

        CRef<CDelta_seq> delta(new CDelta_seq);
        gotcha = 2;

        if (r != q) {
            *r = '\0';
            j  = (Int4)(r - q);

            delta->SetLiteral().SetLength(j);
            delta->SetLiteral().SetSeq_data().SetIupacna().Set(string(q, r));

            deltas.push_back(delta);

            delta.Reset(new CDelta_seq);

            *r = 'N';
        }

        delta->SetLiteral().SetLength(i);
        if (i == 100) {
            delta->SetLiteral().SetFuzz().SetLim();
        }

        deltas.push_back(delta);
        q = p;
    }

    if (p > q) {
        j = (Int4)(p - q);

        CRef<CDelta_seq> delta(new CDelta_seq);
        delta->SetLiteral().SetLength(j);
        delta->SetLiteral().SetSeq_data().SetIupacna().Set(string(q, p));

        deltas.push_back(delta);
    }

    if (deltas.size() > 1) {
        bioseq.SetInst().SetExt().SetDelta().Set().swap(deltas);
        bioseq.SetInst().SetRepr(CSeq_inst::eRepr_delta);
        bioseq.SetInst().ResetSeq_data();
    }

    if (bioseq.GetInst().GetRepr() != CSeq_inst::eRepr_delta && tech == 1) {
        FtaErrPost(SEV_WARNING, ERR_SEQUENCE_HTGWithoutGaps, "This Phase 1 HTG sequence has no runs of 100 "
                                                             "or more N's to indicate gaps between component contigs. "
                                                             "This could be an error, or perhaps sequencing is finished "
                                                             "and this record should not be Phase 1.");
    }

    if (bioseq.GetInst().GetRepr() == CSeq_inst::eRepr_delta) {
        if (tech == 4) /* Phase 0 */
            FtaErrPost(SEV_WARNING, ERR_SEQUENCE_HTGPhaseZeroHasGap, "A Phase 0 HTG record usually consists of several reads "
                                                                     "for one contig, and hence gaps are not expected. But "
                                                                     "this record does have one (ore more) gaps, hence it "
                                                                     "may require review.");
        if (gotcha == 1)
            FtaErrPost(SEV_WARNING, ERR_SEQUENCE_HTGPossibleShortGap, "This sequence has one or more runs "
                                                                      "of at least 20 N's. They could indicate gaps, "
                                                                      "but have not been treated that way because "
                                                                      "they are below the minimum of 100 N's.");
    }
}

/**********************************************************/
static bool fta_ranges_to_hist(const CGB_block::TExtra_accessions& extra_accs)
{
    string ppacc1;
    string ppacc2;
    char*  master;
    char*  range;
    char*  acc1;
    char*  acc2;
    char*  p;
    char*  q;
    Char   ch1;
    Char   ch2;

    if (extra_accs.empty())
        return false;

    if (extra_accs.size() != 2)
        return true;

    CGB_block::TExtra_accessions::const_iterator it = extra_accs.begin();

    ppacc1 = *it;
    ++it;
    ppacc2 = *it;
    acc1   = ppacc1.data();
    acc2   = ppacc2.data();

    if (! acc1 && ! acc2)
        return false;
    if (! acc1 || ! acc2)
        return true;

    p = StringChr(acc1, '-');
    q = StringChr(acc2, '-');

    if (p && q)
        return true;

    if (! p) {
        master = acc1;
        range  = acc2;
        if (q)
            *q = '\0';
    } else {
        master = acc2;
        range  = acc1;
        if (p) // ?
            *p = '\0';
    }

    if (fta_if_wgs_acc(master) != 0 || fta_if_wgs_acc(range) != 1) {
        if (p)
            *p = '-';
        if (q)
            *q = '-';
        return true;
    }

    if (p)
        *p = '-';
    if (q)
        *q = '-';

    for (p = master; *p != '\0' && (*p < '0' || *p > '9');)
        p++;
    if (*p != '\0')
        p++;
    if (*p != '\0')
        p++;
    ch1 = *p;
    *p  = '\0';

    for (q = range; *q != '\0' && (*q < '0' || *q > '9');)
        q++;
    if (*q != '\0')
        q++;
    if (*q != '\0')
        q++;
    ch2 = *q;
    *q  = '\0';

    bool ret = (master == range);
    *p       = ch1;
    *q       = ch2;

    return ret;
}


static bool s_IsConOrScaffold(CBioseq_Handle bsh)
{
    if (bsh &&
        bsh.IsSetInst_Repr() &&
        bsh.GetInst_Repr() == CSeq_inst::eRepr_delta &&
        bsh.IsSetInst_Ext()) {
        const auto& ext = bsh.GetInst_Ext();
        if (ext.IsDelta() &&
            ext.GetDelta().IsSet()) {
            const auto& delta = ext.GetDelta().Get();
            return any_of(begin(delta),
                          end(delta),
                          [](CRef<CDelta_seq> pDeltaSeq) { return (pDeltaSeq && pDeltaSeq->IsLoc()); });
        }
    }
    return false;
}

static bool s_IsAccession(const CSeq_id& id)
{
    const auto idType = id.Which();
    switch (idType) {
    case CSeq_id::e_Local:
    case CSeq_id::e_General:
    case CSeq_id::e_Gi:
    case CSeq_id::e_Named_annot_track:
        return false;
    default:
        return true;
    }
}


bool g_DoesNotReferencePrimary(const CDelta_ext& delta_ext, const CSeq_id& primary, CScope& scope)
{
    const auto primaryType        = primary.Which();
    string     primaryString      = primary.GetSeqIdString();
    const bool primaryIsAccession = s_IsAccession(primary);
    const bool primaryIsGi        = primaryIsAccession ? false : (primaryType == CSeq_id::e_Gi);

    unique_ptr<string> pPrimaryAccessionString;

    for (const auto& pDeltaSeq : delta_ext.Get()) {
        if (pDeltaSeq && pDeltaSeq->IsLoc()) {
            auto        pId         = pDeltaSeq->GetLoc().GetId();
            const auto& deltaIdType = pId->Which();
            if (deltaIdType == primaryType) {
                if (pId->GetSeqIdString() == primaryString) {
                    return false;
                }
            } else {
                if (primaryIsAccession && deltaIdType == CSeq_id::e_Gi) {
                    auto deltaHandle          = CSeq_id_Handle::GetHandle(pId->GetGi());
                    auto deltaAccessionHandle = scope.GetAccVer(deltaHandle);
                    if (! deltaAccessionHandle) {
                        return false;
                    }

                    if (deltaAccessionHandle.GetSeqId()->GetSeqIdString() ==
                        primaryString) {
                        return false;
                    }
                } else if (primaryIsGi && s_IsAccession(*pId)) {
                    if (! pPrimaryAccessionString) {
                        auto primaryGiHandle        = CSeq_id_Handle::GetHandle(primary.GetGi());
                        auto primaryAccessionHandle = scope.GetAccVer(primaryGiHandle);
                        if (! primaryAccessionHandle) {
                            return false;
                        }
                        pPrimaryAccessionString =
                            make_unique<string>(primaryAccessionHandle.GetSeqId()->GetSeqIdString());
                    }

                    if (*pPrimaryAccessionString == pId->GetSeqIdString()) {
                        return false;
                    }
                }
            }
        }
    }
    return true;
}


static int sGetPrefixLength(string_view accession)
{
    auto it = find_if(begin(accession),
                      end(accession),
                      [](char c) { return ! (isalpha(c) || c == '_'); });

    _ASSERT(it != accession.end());
    return int(distance(accession.begin(), it));
}


/**********************************************************/
void fta_add_hist(ParserPtr pp, CBioseq& bioseq, CGB_block::TExtra_accessions& extra_accs, Parser::ESource source, CSeq_id::E_Choice acctype, bool pricon, const char* acc)
{
    Int4 pri_acc;
    Int4 sec_acc;

    if (pp->accver == false || pp->histacc == false ||
        pp->source != source || pp->entrez_fetch == 0)
        return;

    if (! fta_ranges_to_hist(extra_accs))
        return;

    CGB_block::TExtra_accessions hist;
    UnwrapAccessionRange(extra_accs, hist);
    if (hist.empty())
        return;

    // IndexblkPtr ibp = pp->entrylist[pp->curindx];

    pri_acc = fta_if_wgs_acc(acc);

    string_view primaryAccession(acc);
    SIZE_TYPE   prefixLength = 0;


    // bulk load sequences
    vector<string> candidatesAccs;
    vector<CRef<CSeq_id>> candidatesIds;
    vector<CSeq_id_Handle> candidatesIdhs;
    
    list<CRef<CSeq_id>> replaces;

    for (const auto& accessionString : hist) {
        if (accessionString.empty())
            continue;

        const auto idChoice = GetNucAccOwner(accessionString);
        if (idChoice == CSeq_id::e_not_set) {
            continue;
        }
        sec_acc = fta_if_wgs_acc(accessionString);
        if (sec_acc == 0) { // Project WGS accession
            continue;
        }

        if (sec_acc == 1) // Contig WGS accession
        {
            if (pri_acc == 0 || pri_acc == 2) { // A project WGS accession or
                continue;                       // a scaffold WGS accession
            }

            if (pri_acc == 1) { // Contig WGS accession
                if (prefixLength <= 0) {
                    prefixLength = sGetPrefixLength(primaryAccession);
                }

                if ((accessionString.length() <= prefixLength ||
                     ! NStr::EqualNocase(accessionString, 0, prefixLength, primaryAccession.substr(0, prefixLength)) ||
                     ! isdigit(accessionString[prefixLength])) &&
                    ! pp->allow_uwsec) {
                    continue;
                }
            }
        }

        CRef<CSeq_id> id(new CSeq_id(idChoice, accessionString));
        candidatesAccs.push_back(accessionString);
        candidatesIds.push_back(id);
        candidatesIdhs.push_back(CSeq_id_Handle::GetHandle(*id));
    }

    vector<CBioseq_Handle> secondaryBshs = GetScope().GetBioseqHandles(candidatesIdhs);
    for ( size_t i = 0; i < candidatesIdhs.size(); ++i ) {
        auto&         accessionString = candidatesAccs[i];
        auto          id = candidatesIds[i];
        auto          idChoice = id->Which();
        auto          secondaryBsh    = secondaryBshs[i];
        bool          IsConOrScaffold = false;
        try {
            IsConOrScaffold = s_IsConOrScaffold(secondaryBsh);
        } catch (...) {
            FtaErrPost(SEV_ERROR, ERR_ACCESSION_CannotGetDivForSecondary, 
                    "Failed to determine division code for secondary accession \"{}\". Entry dropped.",
                    accessionString);
            continue;
        }

        if (! IsConOrScaffold && pricon && idChoice == acctype) {
            continue;
        }

        if (IsConOrScaffold && ! pricon) {
            CRef<CSeq_id> pPrimary(new CSeq_id(primaryAccession));
            if (g_DoesNotReferencePrimary(secondaryBsh.GetInst_Ext().GetDelta(),
                                          *pPrimary,
                                          GetScope())) {
                replaces.push_back(id);
            }
            continue;
        }

        replaces.push_back(id);
    }


    if (! replaces.empty()) {
        auto& hist_replaces_ids = bioseq.SetInst().SetHist().SetReplaces().SetIds();
        hist_replaces_ids.splice(hist_replaces_ids.end(), replaces);
    }
}

/**********************************************************/
bool fta_strings_same(const char* s1, const char* s2)
{
    if (! s1 && ! s2)
        return true;
    if (! s1 || ! s2 || ! StringEqu(s1, s2))
        return false;
    return true;
}

/**********************************************************/
bool fta_check_htg_kwds(TKeywordList& kwds, IndexblkPtr ibp, CMolInfo& mol_info)
{
    bool deldiv = false;

    for (TKeywordList::iterator key = kwds.begin(); key != kwds.end();) {
        bool delnode = false;
        bool errpost = false;
        if (*key == "HTGS_PHASE0") {
            if (ibp->htg != 0 && ibp->htg != 5) {
                delnode = true;
                if (ibp->htg == 1 || ibp->htg == 2 || ibp->htg == 3)
                    errpost = true;
            } else {
                ibp->htg = 4;
                mol_info.SetTech(CMolInfo::eTech_htgs_0);
            }
            deldiv = true;
        } else if (*key == "HTGS_PHASE1") {
            if (ibp->htg != 0 && ibp->htg != 5) {
                delnode = true;
                if (ibp->htg == 2 || ibp->htg == 3 || ibp->htg == 4)
                    errpost = true;
            } else {
                ibp->htg = 1;
                mol_info.SetTech(CMolInfo::eTech_htgs_1);
            }
            deldiv = true;
        } else if (*key == "HTGS_PHASE2") {
            if (ibp->htg != 0 && ibp->htg != 5) {
                delnode = true;
                if (ibp->htg == 1 || ibp->htg == 3 || ibp->htg == 4)
                    errpost = true;
            } else {
                ibp->htg = 2;
                mol_info.SetTech(CMolInfo::eTech_htgs_2);
            }
            deldiv = true;
        } else if (*key == "HTGS_PHASE3") {
            if (ibp->htg != 0 && ibp->htg != 5) {
                delnode = true;
                if (ibp->htg == 1 || ibp->htg == 2 || ibp->htg == 4)
                    errpost = true;
            } else {
                ibp->htg = 3;
                mol_info.SetTech(CMolInfo::eTech_htgs_3);
            }
            deldiv = true;
        } else if (*key == "HTG") {
            if (ibp->htg == 0) {
                ibp->htg = 5;
                mol_info.SetTech(CMolInfo::eTech_htgs_3);
            }
            deldiv = true;
        }

        if (errpost) {
            FtaErrPost(SEV_ERROR, ERR_KEYWORD_MultipleHTGPhases, "This entry has multiple HTG-related keywords, for differing HTG phases. Ignoring all but the first.");
        }

        if (delnode)
            key = kwds.erase(key);
        else
            ++key;
    }
    if (ibp->htg == 5)
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

    if (! ftbp || length < 1)
        return;

    ftsp       = new FTATpaSpan;
    ftsp->from = ftbp->from1;
    ftsp->to   = ftbp->to1;
    ftsp->next = nullptr;
    tftsp      = ftsp;
    for (tftbp = ftbp; tftbp; tftbp = tftbp->next) {
        i1 = tftbp->to1 - tftbp->from1;
        i2 = tftbp->to2 - tftbp->from2;
        j  = (i2 > i1) ? (i2 - i1) : (i1 - i2);
        i1++;

        if (i1 < 3000 && j * 10 > i1) {
            if (tpa)
                FtaErrPost(SEV_ERROR, ERR_TPA_SpanLengthDiff, 
                        "Span \"{}..{}\" of this TPA record differs from the span \"{}..{}\" of the contributing primary sequence or trace record by more than 10 percent.", 
                        tftbp->from1, tftbp->to1, tftbp->from2, tftbp->to2);
            else
                FtaErrPost(SEV_ERROR, ERR_TSA_SpanLengthDiff, 
                        "Span \"{}..{}\" of this TSA record differs from the span \"{}..{}\" of the contributing primary sequence or trace record by more than 10 percent.", 
                        tftbp->from1, tftbp->to1, tftbp->from2, tftbp->to2);
        }

        if (i1 >= 3000 && j > 300) {
            if (tpa)
                FtaErrPost(SEV_ERROR, ERR_TPA_SpanDiffOver300bp, 
                        "Span \"{}..{}\" of this TPA record differs from span \"{}..{}\" of the contributing primary sequence or trace record by more than 300 basepairs.", 
                        tftbp->from1, tftbp->to1, tftbp->from2, tftbp->to2);
            else
                FtaErrPost(SEV_ERROR, ERR_TSA_SpanDiffOver300bp, 
                        "Span \"{}..{}\" of this TSA record differs from span \"{}..{}\" of the contributing primary sequence or trace record by more than 300 basepairs.", 
                        tftbp->from1, tftbp->to1, tftbp->from2, tftbp->to2);
        }

        if (tftbp->from1 <= tftsp->to + 1) {
            if (tftbp->to1 > tftsp->to)
                tftsp->to = tftbp->to1;
            continue;
        }

        tftsp->next = new FTATpaSpan;
        tftsp       = tftsp->next;
        tftsp->from = tftbp->from1;
        tftsp->to   = tftbp->to1;
    }

    if (ftsp->from - 1 > 50) {
        if (tpa)
            FtaErrPost(SEV_ERROR, ERR_TPA_IncompleteCoverage, "This TPA record contains a sequence region \"1..{}\" greater than 50 basepairs long that is not accounted for by a contributing primary sequence or trace record.", ftsp->from - 1);
        else
            FtaErrPost(SEV_ERROR, ERR_TSA_IncompleteCoverage, "This TSA record contains a sequence region \"1..{}\" greater than 50 basepairs long that is not accounted for by a contributing primary sequence or trace record.", ftsp->from - 1);
    }

    for (; ftsp; ftsp = tftsp) {
        tftsp = ftsp->next;
        if (tftsp && tftsp->from - ftsp->to - 1 > 50) {
            if (tpa)
                FtaErrPost(SEV_ERROR, ERR_TPA_IncompleteCoverage, "This TPA record contains a sequence region \"{}..{}\" greater than 50 basepairs long that is not accounted for by a contributing primary sequence or trace record.", ftsp->to + 1, tftsp->from - 1);
            else
                FtaErrPost(SEV_ERROR, ERR_TSA_IncompleteCoverage, "This TSA record contains a sequence region \"{}..{}\" greater than 50 basepairs long that is not accounted for by a contributing primary sequence or trace record.", ftsp->to + 1, tftsp->from - 1);
        } else if (! tftsp && length - ftsp->to > 50) {
            if (tpa)
                FtaErrPost(SEV_ERROR, ERR_TPA_IncompleteCoverage, "This TPA record contains a sequence region \"{}..{}\" greater than 50 basepairs long that is not accounted for by a contributing primary sequence or trace record.", ftsp->to + 1, length);
            else
                FtaErrPost(SEV_ERROR, ERR_TSA_IncompleteCoverage, "This TSA record contains a sequence region \"{}..{}\" greater than 50 basepairs long that is not accounted for by a contributing primary sequence or trace record.", ftsp->to + 1, length);
        }

        delete ftsp;
    }
}

/**********************************************************/
bool fta_number_is_huge(const Char* s)
{
    size_t i = StringLen(s);
    if (i > 10)
        return true;
    else if (i < 10)
        return false;

    if (*s > '2')
        return true;
    else if (*s < '2')
        return false;

    if (*++s > '1')
        return true;
    else if (*s < '1')
        return false;

    if (*++s > '4')
        return true;
    else if (*s < '4')
        return false;

    if (*++s > '7')
        return true;
    else if (*s < '7')
        return false;

    if (*++s > '4')
        return true;
    else if (*s < '4')
        return false;

    if (*++s > '8')
        return true;
    else if (*s < '8')
        return false;

    if (*++s > '3')
        return true;
    else if (*s < '3')
        return false;

    if (*++s > '6')
        return true;
    else if (*s < '6')
        return false;

    if (*++s > '4')
        return true;
    else if (*s < '4')
        return false;

    if (*++s > '7')
        return true;
    return false;
}

/**********************************************************/
bool fta_parse_tpa_tsa_block(CBioseq& bioseq, char* offset, char* acnum, Int2 vernum, size_t len, Int2 col_data, bool tpa)
{
    FTATpaBlockPtr ftbp;
    FTATpaBlockPtr tftbp;
    FTATpaBlockPtr ft;

    string      buf;
    char*       p;
    char*       q;
    char*       r;
    char*       t;
    const char* bad_accession;
    bool        bad_line;
    bool        bad_interval;
    Int4        from1;
    Int4        to1;
    Int4        len1;
    Int4        len2;

    CSeq_id::E_Choice choice;

    if (! offset || ! acnum || len < 2)
        return false;

    choice = GetNucAccOwner(acnum);

    if (col_data == 0) /* HACK: XML format */
    {
        for (p = offset; *p != '\0'; p++)
            if (*p == '~')
                *p = '\n';
        p = StringChr(offset, '\n');
        if (! p)
            return false;
        buf.assign(p + 1);
        buf.append("\n");
    } else {
        p = SrchTheChar(offset, offset + len, '\n');
        if (! p) {
            return false;
        }
        buf.assign(p + 1, offset + len);
    }

    ftbp = new FTATpaBlock;

    bad_line      = false;
    bad_interval  = false;
    bad_accession = nullptr;
    p             = buf.data();
    for (q = StringChr(p, '\n'); q; p = q + 1, q = StringChr(p, '\n')) {
        *q = '\0';
        if ((Int2)StringLen(p) < col_data)
            break;
        for (p += col_data; *p == ' ';)
            p++;
        for (r = p; *p >= '0' && *p <= '9';)
            p++;
        if (*p != '-') {
            bad_interval = true;
            break;
        }

        *p++  = '\0';
        from1 = atoi(r);

        for (r = p; *p >= '0' && *p <= '9';)
            p++;
        if (*p != ' ' && *p != '\n' && *p != '\0') {
            bad_interval = true;
            break;
        }
        if (*p != '\0')
            *p++ = '\0';
        to1 = atoi(r);

        if (from1 >= to1) {
            bad_interval = true;
            break;
        }

        for (ft = ftbp; ft->next; ft = ft->next)
            if ((ft->next->from1 > from1) ||
                (ft->next->from1 == from1 && ft->next->to1 > to1))
                break;
        tftbp       = new FTATpaBlock;
        tftbp->next = ft->next;
        ft->next    = tftbp;

        tftbp->from1 = from1;
        tftbp->to1   = to1;

        while (*p == ' ')
            p++;
        for (r = p; *p != '\0' && *p != ' ' && *p != '\n';)
            p++;
        if (*p != '\0')
            *p++ = '\0';
        tftbp->accession = StringSave(r);
        r                = StringChr(tftbp->accession, '.');
        if (r) {
            *r++ = '\0';
            for (t = r; *t >= '0' && *t <= '9';)
                t++;
            if (*t != '\0') {
                *--r          = '.';
                bad_accession = tftbp->accession;
                break;
            }
            tftbp->version = atoi(r);
        }

        if (StringEquNI(tftbp->accession, "ti", 2)) {
            for (r = tftbp->accession + 2; *r == '0';)
                r++;
            if (*r == '\0') {
                bad_accession = tftbp->accession;
                break;
            }
            while (*r >= '0' && *r <= '9')
                r++;
            if (*r != '\0') {
                bad_accession = tftbp->accession;
                break;
            }
        } else {
            tftbp->sicho = GetNucAccOwner(tftbp->accession);
            if ((tftbp->sicho != CSeq_id::e_Genbank && tftbp->sicho != CSeq_id::e_Embl &&
                 tftbp->sicho != CSeq_id::e_Ddbj &&
                 (tftbp->sicho != CSeq_id::e_Tpg || tpa == false))) {
                bad_accession = tftbp->accession;
                break;
            }
        }

        while (*p == ' ')
            p++;

        if (StringEquNI(p, "not_available", 13)) {
            p += 13;
            tftbp->from2 = 1;
            tftbp->to2   = 1;
        } else {
            for (r = p; *p >= '0' && *p <= '9';)
                p++;
            if (*p != '-') {
                bad_interval = true;
                break;
            }
            *p++         = '\0';
            tftbp->from2 = atoi(r);

            for (r = p; *p >= '0' && *p <= '9';)
                p++;
            if (*p != ' ' && *p != '\n' && *p != '\0') {
                bad_interval = true;
                break;
            }
            if (*p != '\0')
                *p++ = '\0';
            tftbp->to2 = atoi(r);

            if (tftbp->from2 >= tftbp->to2) {
                bad_interval = true;
                break;
            }
        }

        while (*p == ' ')
            p++;
        if (*p == 'c') {
            tftbp->strand = eNa_strand_minus;
            for (p++; *p == ' ';)
                p++;
        } else
            tftbp->strand = eNa_strand_plus;
        if (*p != '\0') {
            bad_line = true;
            break;
        }
    }

    buf.clear();
    if (bad_line || bad_interval || bad_accession) {
        if (bad_interval) {
            if (tpa)
                FtaErrPost(SEV_REJECT, ERR_TPA_InvalidPrimarySpan, "Intervals from primary records on which a TPA record is based must be of form X-Y, where X is less than Y and both X and Y are integers. Entry dropped.");
            else
                FtaErrPost(SEV_REJECT, ERR_TSA_InvalidPrimarySpan, "Intervals from primary records on which a TSA record is based must be of form X-Y, where X is less than Y and both X and Y are integers. Entry dropped.");
        } else if (bad_accession) {
            if (tpa)
                FtaErrPost(SEV_REJECT, ERR_TPA_InvalidPrimarySeqId, "\"{}\" is not a GenBank/EMBL/DDBJ/Trace sequence identifier. Entry dropped.", bad_accession);
            else
                FtaErrPost(SEV_REJECT, ERR_TSA_InvalidPrimarySeqId, "\"{}\" is not a GenBank/EMBL/DDBJ/Trace sequence identifier. Entry dropped.", bad_accession);
        } else {
            if (tpa)
                FtaErrPost(SEV_REJECT, ERR_TPA_InvalidPrimaryBlock, "Supplied PRIMARY block for TPA record is incorrect. Cannot parse. Entry dropped.");
            else
                FtaErrPost(SEV_REJECT, ERR_TSA_InvalidPrimaryBlock, "Supplied PRIMARY block for TSA record is incorrect. Cannot parse. Entry dropped.");
        }

        if (ftbp)
            fta_tpa_block_free(ftbp);
        return false;
    }

    tftbp      = ftbp->next;
    ftbp->next = nullptr;
    delete ftbp;
    ftbp = tftbp;

    fta_check_tpa_tsa_coverage(ftbp, bioseq.GetLength(), tpa);

    CSeq_hist::TAssembly& assembly = bioseq.SetInst().SetHist().SetAssembly();
    if (! assembly.empty())
        assembly.clear();

    CRef<CSeq_align> root_align(new CSeq_align);

    root_align->SetType(CSeq_align::eType_not_set);
    CSeq_align_set& align_set = root_align->SetSegs().SetDisc();

    for (; tftbp; tftbp = tftbp->next) {
        len1 = tftbp->to1 - tftbp->from1 + 1;
        len2 = tftbp->to2 - tftbp->from2 + 1;

        CRef<CSeq_align> align(new CSeq_align);
        align->SetType(CSeq_align::eType_partial);
        align->SetDim(2);

        CSeq_align::C_Segs::TDenseg& seg = align->SetSegs().SetDenseg();

        seg.SetDim(2);
        seg.SetNumseg((len1 == len2) ? 1 : 2);

        seg.SetStarts().push_back(tftbp->from1 - 1);
        seg.SetStarts().push_back(tftbp->from2 - 1);

        if (len1 != len2) {
            if (len1 < len2) {
                seg.SetStarts().push_back(-1);
                seg.SetStarts().push_back(tftbp->from2 - 1 + len1);
            } else {
                seg.SetStarts().push_back(tftbp->from1 - 1 + len2);
                seg.SetStarts().push_back(-1);
            }
        }

        if (len1 == len2)
            seg.SetLens().push_back(len1);
        else if (len1 < len2) {
            seg.SetLens().push_back(len1);
            seg.SetLens().push_back(len2 - len1);
        } else {
            seg.SetLens().push_back(len2);
            seg.SetLens().push_back(len1 - len2);
        }

        seg.SetStrands().push_back(eNa_strand_plus);
        seg.SetStrands().push_back(tftbp->strand);

        if (len1 != len2) {
            seg.SetStrands().push_back(eNa_strand_plus);
            seg.SetStrands().push_back(tftbp->strand);
        }

        CRef<CTextseq_id> text_id(new CTextseq_id);
        text_id->SetAccession(acnum);

        if (vernum > 0)
            text_id->SetVersion(vernum);

        CRef<CSeq_id> id(new CSeq_id),
            aux_id;
        SetTextId(choice, *id, *text_id);
        seg.SetIds().push_back(id);

        if (StringEquNI(tftbp->accession, "ti", 2)) {
            CRef<CSeq_id> gen_id(new CSeq_id);
            CDbtag&       tag = gen_id->SetGeneral();

            for (r = tftbp->accession + 2; *r == '0';)
                r++;
            if (fta_number_is_huge(r) == false)
                tag.SetTag().SetId(atoi(r));
            else
                tag.SetTag().SetStr(r);

            tag.SetDb("ti");
            seg.SetIds().push_back(gen_id);
        } else {
            CRef<CTextseq_id> otext_id(new CTextseq_id);
            otext_id->SetAccession(tftbp->accession);

            if (tftbp->version > 0)
                otext_id->SetVersion(tftbp->version);

            aux_id.Reset(new CSeq_id);
            SetTextId(tftbp->sicho, *aux_id, *otext_id);
        }

        if (aux_id.NotEmpty())
            seg.SetIds().push_back(aux_id);

        align_set.Set().push_back(align);
    }

    assembly.push_back(root_align);

    if (ftbp)
        fta_tpa_block_free(ftbp);
    return true;
}

/**********************************************************/
char* StringRStr(char* where, const char* what)
{
    if (! where || ! what || *where == '\0' || *what == '\0')
        return nullptr;

    size_t i   = StringLen(what);
    char*  res = nullptr;
    for (char* p = where; *p != '\0'; p++)
        if (StringEquN(p, what, i))
            res = p;

    return (res);
}

/**********************************************************/
CRef<CSeq_loc> fta_get_seqloc_int_whole(const CSeq_id& seq_id, size_t len)
{
    CRef<CSeq_loc> ret;

    if (len < 1)
        return ret;

    ret.Reset(new CSeq_loc);
    CSeq_interval& interval = ret->SetInt();

    interval.SetFrom(0);
    interval.SetTo(static_cast<TSeqPos>(len) - 1);
    interval.SetId().Assign(seq_id);

    return ret;
}

/**********************************************************/
static void fta_validate_assembly(char* name)
{
    bool bad_format = false;

    char* p = name;
    if (! p || *p == '\0' || StringLen(p) < 7)
        bad_format = true;
    else if (p[0] != 'G' || p[1] != 'C' || (p[2] != 'F' && p[2] != 'A') ||
             p[3] != '_' || p[4] < '0' || p[4] > '9')
        bad_format = true;
    else {
        for (p += 5; *p != '\0'; p++)
            if (*p < '0' || *p > '9')
                break;
        if (*p != '.' || p[1] < '0' || p[1] > '9')
            bad_format = true;
        else {
            for (p++; *p != '\0'; p++)
                if (*p < '0' || *p > '9')
                    break;
            if (*p != '\0')
                bad_format = true;
        }
    }

    if (bad_format)
        FtaErrPost(SEV_WARNING, ERR_DBLINK_InvalidIdentifier, "\"{}\" is not a validly formatted identifier for the Assembly resource.", name);
}

/**********************************************************/
static bool fta_validate_bioproject(char* name, Parser::ESource source)
{
    char* p;
    bool  bad_format = false;

    if (StringLen(name) < 6)
        bad_format = true;
    else if (name[0] != 'P' || name[1] != 'R' || name[2] != 'J' ||
             (name[3] != 'E' && name[3] != 'N' && name[3] != 'D') ||
             name[4] < 'A' || name[4] > 'Z' || name[5] < '0' || name[5] > '9')
        bad_format = true;
    else {
        for (p = name + 6; *p != '\0'; p++)
            if (*p < '0' || *p > '9')
                break;
        if (*p != '\0')
            bad_format = true;
    }

    if (bad_format) {
        FtaErrPost(SEV_REJECT, ERR_FORMAT_InvalidBioProjectAcc, "BioProject accession number is not validly formatted: \"{}\". Entry dropped.", name);
        return false;
    }

    if ((source == Parser::ESource::NCBI && name[3] != 'N') ||
        (source == Parser::ESource::DDBJ && name[3] != 'D' &&
         (name[3] != 'N' || name[4] != 'A')) ||
        (source == Parser::ESource::EMBL && name[3] != 'E' &&
         (name[3] != 'N' || name[4] != 'A')))
        FtaErrPost(SEV_WARNING, ERR_FORMAT_WrongBioProjectPrefix, "BioProject accession number does not agree with this record's database of origin: \"{}\".", name);

    return true;
}

/**********************************************************/
static ValNodePtr fta_tokenize_project(char* str, Parser::ESource source, bool newstyle)
{
    ValNodePtr vnp;
    ValNodePtr tvnp;
    char*      p;
    char*      q;
    char*      r;
    bool       bad;
    Char       ch;

    if (! str || *str == '\0') {
        FtaErrPost(SEV_REJECT, ERR_FORMAT_InvalidBioProjectAcc, "Empty PROJECT/PR line type supplied. Entry dropped.");
        return nullptr;
    }

    for (p = str; *p != '\0'; p++)
        if (*p == ';' || *p == ',' || *p == '\t')
            *p = ' ';

    for (p = str; *p == ' ';)
        p++;
    if (*p == '\0') {
        FtaErrPost(SEV_REJECT, ERR_FORMAT_InvalidBioProjectAcc, "Empty PROJECT/PR line type supplied. Entry dropped.");
        return nullptr;
    }

    vnp  = ValNodeNew(nullptr);
    tvnp = vnp;

    for (bad = false, p = str; *p != '\0';) {
        while (*p == ' ')
            p++;

        if (*p == '\0')
            break;

        for (q = p; *p != ' ' && *p != '\0';)
            p++;

        ch = *p;
        *p = '\0';
        if (! newstyle) {
            for (r = q; *r >= '0' && *r <= '9';)
                r++;
            if (*r != '\0') {
                FtaErrPost(SEV_REJECT, ERR_FORMAT_InvalidBioProjectAcc, "BioProject accession number is not validly formatted: \"{}\". Entry dropped.", q);
                bad = true;
            }
        } else if (fta_validate_bioproject(q, source) == false)
            bad = true;

        if (bad) {
            *p = ch;
            break;
        }

        tvnp = ValNodeNew(tvnp, q);
        *p   = ch;
    }

    tvnp = vnp->next;
    delete vnp;

    if (! tvnp)
        return nullptr;

    if (! bad)
        return (tvnp);

    ValNodeFreeData(tvnp);
    return nullptr;
}

/**********************************************************/
void fta_get_project_user_object(TSeqdescList& descrs, char* offset, Parser::EFormat format, bool* drop, Parser::ESource source)
{
    ValNodePtr vnp;
    ValNodePtr tvnp;

    const Char* name;

    char* str;
    char* p;
    Char  ch;
    Int4  i;

    if (! offset)
        return;

    bool newstyle = false;
    if (format == Parser::EFormat::GenBank) {
        i    = ParFlat_COL_DATA;
        name = "GenomeProject:";
        ch   = '\n';
    } else {
        i    = ParFlat_COL_DATA_EMBL;
        name = "Project:";
        ch   = ';';
    }

    size_t len = StringLen(name);
    str        = StringSave(offset + i);
    p          = StringChr(str, ch);
    if (p)
        *p = '\0';

    if (! StringEquN(str, name, len)) {
        if (format == Parser::EFormat::GenBank) {
            FtaErrPost(SEV_REJECT, ERR_FORMAT_InvalidBioProjectAcc, "PROJECT line is missing \"GenomeProject:\" tag. Entry dropped."); // str
            MemFree(str);
            *drop = true;
            return;
        }
        newstyle = true;
        len      = 0;
    } else if (format == Parser::EFormat::EMBL && str[len] == 'P')
        newstyle = true;

    vnp = fta_tokenize_project(str + len, source, newstyle);
    if (! vnp) {
        *drop = true;
        MemFree(str);
        return;
    }

    CUser_object* user_obj_ptr;
    bool          got = false;

    for (auto& descr : descrs) {
        if (! descr->IsUser() || ! descr->GetUser().IsSetData())
            continue;

        user_obj_ptr = &(descr->SetUser());

        CObject_id* obj_id = nullptr;
        if (user_obj_ptr->IsSetType())
            obj_id = &(user_obj_ptr->SetType());

        if (obj_id && obj_id->IsStr() && obj_id->GetStr() == "DBLink") {
            got = true;
            break;
        }
    }

    CRef<CUser_object> user_obj;
    if (newstyle) {
        for (i = 0, tvnp = vnp; tvnp; tvnp = tvnp->next)
            i++;

        if (! got) {
            user_obj.Reset(new CUser_object);
            user_obj_ptr = user_obj.GetNCPointer();

            CObject_id& id = user_obj_ptr->SetType();
            id.SetStr("DBLink");
        }

        CRef<CUser_field> user_field(new CUser_field);
        user_field->SetLabel().SetStr("BioProject");
        user_field->SetNum(i);

        for (tvnp = vnp; tvnp; tvnp = tvnp->next)
            user_field->SetData().SetStrs().push_back(tvnp->data);

        user_obj_ptr->SetData().push_back(user_field);
    } else {
        got = false;

        user_obj.Reset(new CUser_object);
        user_obj_ptr = user_obj.GetNCPointer();

        CObject_id& id = user_obj_ptr->SetType();
        id.SetStr("GenomeProjectsDB");

        for (tvnp = vnp; tvnp; tvnp = tvnp->next) {

            CRef<CUser_field> user_field(new CUser_field);
            user_field->SetLabel().SetStr("ProjectID");
            user_field->SetData().SetInt(atoi(tvnp->data));
            user_obj_ptr->SetData().push_back(user_field);


            user_field.Reset(new CUser_field);
            user_field->SetLabel().SetStr("ParentID");
            user_field->SetData().SetInt(0);
            user_obj_ptr->SetData().push_back(user_field);
        }
    }

    if (! got) {
        CRef<CSeqdesc> descr(new CSeqdesc);
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

    if (p && StringLen(p) > 3 &&
        (p[0] == 'E' || p[0] == 'S' || p[0] == 'D') && p[1] == 'R' &&
        (p[2] == 'A' || p[2] == 'P' || p[2] == 'R' || p[2] == 'S' ||
         p[2] == 'X' || p[2] == 'Z')) {
        for (p += 3; *p >= '0' && *p <= '9';)
            p++;
        if (*p == '\0')
            return true;
    }

    if (dblink)
        FtaErrPost(SEV_REJECT, ERR_FORMAT_IncorrectDBLINK, "Incorrectly formatted DBLINK Sequence Read Archive value: \"{}\". Entry dropped.", id);

    return false;
}

/**********************************************************/
bool fta_if_valid_biosample(const Char* id, bool dblink)
{
    const Char* p = id;

    if (p && StringLen(p) > 5 && p[0] == 'S' && p[1] == 'A' &&
        p[2] == 'M' && (p[3] == 'N' || p[3] == 'E' || p[3] == 'D')) {
        if (p[4] == 'A' || p[4] == 'G')
            p += 5;
        else
            p += 4;
        while (*p >= '0' && *p <= '9')
            p++;
        if (*p == '\0')
            return true;
    }

    if (dblink)
        FtaErrPost(SEV_REJECT, ERR_FORMAT_IncorrectDBLINK, "Incorrectly formatted DBLINK BioSample value: \"{}\". Entry dropped.", id);

    return false;
}

/**********************************************************/
static ValNodePtr fta_tokenize_dblink(char* str, Parser::ESource source)
{
    ValNodePtr vnp;
    ValNodePtr tvnp;
    ValNodePtr uvnp;
    ValNodePtr tagvnp;

    bool got_nl;
    bool bad;
    bool sra;
    bool assembly;
    bool biosample;
    bool bioproject;

    char* p;
    char* q;
    char* r = nullptr;
    char* t;
    char* u;
    Char  ch;

    if (! str || *str == '\0') {
        FtaErrPost(SEV_REJECT, ERR_FORMAT_IncorrectDBLINK, "Empty DBLINK line type supplied. Entry dropped.");
        return nullptr;
    }

    for (p = str; *p != '\0'; p++)
        if (*p == ';' || *p == '\t')
            *p = ' ';

    vnp        = ValNodeNew(nullptr);
    tvnp       = vnp;
    bad        = false;
    got_nl     = true;
    sra        = false;
    assembly   = false;
    biosample  = false;
    bioproject = false;
    tagvnp     = nullptr;

    for (p = str; *p != '\0'; got_nl = false) {
        while (*p == ' ' || *p == '\n' || *p == ':' || *p == ',') {
            if (*p == '\n')
                got_nl = true;
            p++;
        }

        if (got_nl) {
            t = StringChr(p, ':');
            if (t) {
                r = StringChr(p, '\n');
                u = StringChr(p, ',');

                if ((! u || u > t) && (! r || r > t)) {
                    ch = *++t;
                    *t = '\0';

                    if (! StringEqu(p, "Project:") &&
                        ! StringEqu(p, "Assembly:") &&
                        ! StringEqu(p, "BioSample:") &&
                        ! StringEqu(p, "BioProject:") &&
                        ! StringEqu(p, "Sequence Read Archive:") &&
                        ! StringEqu(p, "Trace Assembly Archive:")) {
                        FtaErrPost(SEV_REJECT, ERR_FORMAT_IncorrectDBLINK, "Invalid DBLINK tag encountered: \"{}\". Entry dropped.", p);
                        bad = true;
                        break;
                    }

                    bioproject = StringEqu(p, "BioProject:");
                    sra        = StringEqu(p, "Sequence Read Archive:");
                    biosample  = StringEqu(p, "BioSample:");
                    assembly   = StringEqu(p, "Assembly:");

                    if (tvnp->data && StringChr(tvnp->data, ':')) {
                        FtaErrPost(SEV_REJECT, ERR_FORMAT_IncorrectDBLINK, "Found DBLINK tag with no value: \"{}\". Entry dropped.", tvnp->data);
                        bad = true;
                        break;
                    }

                    for (uvnp = vnp->next; uvnp; uvnp = uvnp->next)
                        if (StringEqu(uvnp->data, p)) {
                            FtaErrPost(SEV_REJECT, ERR_FORMAT_IncorrectDBLINK, "Multiple DBLINK tags found: \"{}\". Entry dropped.", p);
                            bad = true;
                            break;
                        }
                    if (bad)
                        break;

                    tvnp   = ValNodeNew(tvnp, p);
                    tagvnp = tvnp;
                    *t     = ch;
                    p      = t;
                    continue;
                }
            }
        }

        q = p;
        while (*p != ',' && *p != '\n' && *p != ':' && *p != '\0')
            p++;
        if (*p == ':') {
            while (*p != '\0' && *p != '\n')
                p++;
            ch = *p;
            *p = '\0';
            while (*r != '\n' && r > str)
                r--;
            while (*r == ' ' || *r == '\n')
                r++;
            FtaErrPost(SEV_REJECT, ERR_FORMAT_IncorrectDBLINK, "Too many delimiters/fields for DBLINK line: \"{}\". Entry dropped.", r);
            *p  = ch;
            bad = true;
            break;
        }

        if (q == p)
            continue;

        ch = *p;
        *p = '\0';

        if (tagvnp && tagvnp->data) {
            for (uvnp = tagvnp->next; uvnp; uvnp = uvnp->next) {
                if (! uvnp->data || ! StringEqu(uvnp->data, q))
                    continue;

                FtaErrPost(SEV_WARNING, ERR_DBLINK_DuplicateIdentifierRemoved, "Duplicate identifier \"{}\" from \"{}\" link removed.", q, tagvnp->data);
                break;
            }

            if (uvnp) {
                *p = ch;
                continue;
            }
        }

        if ((bioproject &&
             fta_validate_bioproject(q, source) == false) ||
            (biosample && fta_if_valid_biosample(q, true) == false) ||
            (sra && fta_if_valid_sra(q, true) == false)) {
            *p  = ch;
            bad = true;
        }

        if (assembly)
            fta_validate_assembly(q);

        tvnp = ValNodeNew(tvnp, q);
        *p   = ch;
    }

    if (! bad && tvnp->data && StringChr(tvnp->data, ':')) {
        FtaErrPost(SEV_REJECT, ERR_FORMAT_IncorrectDBLINK, "Found DBLINK tag with no value: \"{}\". Entry dropped.", tvnp->data);
        bad = true;
    }

    tvnp = vnp->next;
    delete vnp;

    if (! tvnp)
        return nullptr;

    if (! bad)
        return (tvnp);

    ValNodeFreeData(tvnp);
    return nullptr;
}

/**********************************************************/
void fta_get_dblink_user_object(TSeqdescList& descrs, char* offset, size_t len, Parser::ESource source, bool* drop, CRef<CUser_object>& dbuop)
{
    ValNodePtr vnp;
    ValNodePtr tvnp;
    ValNodePtr uvnp;

    const char* str;
    Int4        i;

    if (! offset)
        return;

    char* str1                   = StringSave(offset + ParFlat_COL_DATA);
    str1[len - ParFlat_COL_DATA] = '\0';
    vnp                          = fta_tokenize_dblink(str1, source);
    MemFree(str1);

    if (! vnp) {
        *drop = true;
        return;
    }

    CRef<CUser_object> user_obj;
    CRef<CUser_field>  user_field;

    for (tvnp = vnp; tvnp; tvnp = tvnp->next) {
        if (StringChr(tvnp->data, ':')) {
            if (user_obj.NotEmpty())
                break;

            if (StringEqu(tvnp->data, "Project:")) {
                user_obj.Reset(new CUser_object);
                CObject_id& id = user_obj->SetType();

                id.SetStr("GenomeProjectsDB");
            }
            continue;
        }

        if (user_obj.Empty())
            continue;

        str = tvnp->data;
        if (! str || *str == '\0')
            continue;

        if (*str != '0')
            while (*str >= '0' && *str <= '9')
                str++;
        if (*str != '\0') {
            FtaErrPost(SEV_ERROR, ERR_FORMAT_IncorrectDBLINK, "Skipping invalid \"Project:\" value on the DBLINK line: \"{}\".", tvnp->data);
            continue;
        }

        user_field.Reset(new CUser_field);

        user_field->SetLabel().SetStr("ProjectID");
        user_field->SetData().SetInt(atoi(tvnp->data));
        user_obj->SetData().push_back(user_field);

        user_field.Reset(new CUser_field);
        user_field->SetLabel().SetStr("ParentID");
        user_field->SetData().SetInt(0);

        user_obj->SetData().push_back(user_field);
    }

    if (user_obj.NotEmpty() && ! user_obj->IsSetData()) {
        user_obj.Reset();
    }

    if (user_obj.NotEmpty()) {
        CRef<CSeqdesc> descr(new CSeqdesc);
        descr->SetUser(*user_obj);
        descrs.push_back(descr);
    }

    user_obj.Reset();
    user_field.Reset();

    bool inpr = false;
    for (tvnp = vnp; tvnp; tvnp = tvnp->next) {
        if (StringChr(tvnp->data, ':')) {
            if (StringEqu(tvnp->data, "Project:")) {
                inpr = true;
                continue;
            }

            inpr = false;

            if (user_obj.Empty()) {
                user_obj.Reset(new CUser_object);
                user_obj->SetType().SetStr("DBLink");
            }

            for (i = 0, uvnp = tvnp->next; uvnp; uvnp = uvnp->next, i++)
                if (StringChr(uvnp->data, ':'))
                    break;

            user_field.Reset(new CUser_field);

            string lstr(tvnp->data);
            lstr = lstr.substr(0, lstr.size() - 1);
            user_field->SetLabel().SetStr(lstr);
            user_field->SetNum(i);
            user_field->SetData().SetStrs();

            user_obj->SetData().push_back(user_field);
        } else if (! inpr && user_obj.NotEmpty()) {
            user_field->SetData().SetStrs().push_back(tvnp->data);
        }
    }

    ValNodeFreeData(vnp);

    if (user_obj.NotEmpty()) {
        CRef<CSeqdesc> descr(new CSeqdesc);
        descr->SetUser(*user_obj);
        descrs.push_back(descr);

        dbuop = user_obj;
    }
}

/**********************************************************/
CMolInfo::TTech fta_check_con_for_wgs(CBioseq& bioseq)
{
    if (bioseq.GetInst().GetRepr() != CSeq_inst::eRepr_delta || ! bioseq.GetInst().IsSetExt() || ! bioseq.GetInst().GetExt().IsDelta())
        return CMolInfo::eTech_unknown;

    bool good     = false;
    bool finished = true;

    for (const auto& delta : bioseq.GetInst().GetExt().GetDelta().Get()) {
        if (! delta->IsLoc())
            continue;

        const CSeq_loc& locs = delta->GetLoc();
        CSeq_loc_CI     ci(locs);

        for (; ci; ++ci) {
            const CSeq_id* id = nullptr;

            CConstRef<CSeq_loc> loc = ci.GetRangeAsSeq_loc();
            if (loc->IsEmpty() || loc->IsWhole() || loc->IsInt() || loc->IsPnt() || loc->IsPacked_pnt())
                id = &ci.GetSeq_id();
            else
                continue;

            if (! id)
                break;

            if (! id->IsGenbank() && ! id->IsEmbl() &&
                ! id->IsOther() && ! id->IsDdbj() &&
                ! id->IsTpg() && ! id->IsTpe() && ! id->IsTpd())
                break;

            const CTextseq_id* text_id = id->GetTextseq_Id();
            if (! text_id || ! text_id->IsSetAccession() ||
                text_id->GetAccession().empty() ||
                fta_if_wgs_acc(text_id->GetAccession()) != 1)
                break;
            good = true;
        }

        if (ci) {
            finished = false;
            break;
        }
    }

    if (good && finished)
        return CMolInfo::eTech_wgs;

    return CMolInfo::eTech_unknown;
}

/**********************************************************/
static void fta_fix_seq_id(CSeq_loc& loc, CSeq_id& id, IndexblkPtr ibp, string_view location, string_view name, SeqLocIdsPtr slip, bool iscon, Parser::ESource source)
{
    Int4 i;

    if (! ibp)
        return;

    if (id.IsLocal()) {
        return;
    }

    if (name.empty() && id.IsGeneral()) {
        const CDbtag& tag = id.GetGeneral();
        if (tag.GetDb() == "SeqLit" || tag.GetDb() == "UnkSeqLit")
            return;
    }

    if (! id.IsGenbank() && ! id.IsEmbl() && ! id.IsPir() &&
        ! id.IsSwissprot() && ! id.IsOther() && ! id.IsDdbj() && ! id.IsPrf() &&
        ! id.IsTpg() && ! id.IsTpe() && ! id.IsTpd()) {

        if (name.empty())
            FtaErrPost(SEV_REJECT, ERR_LOCATION_SeqIdProblem, "Empty or unsupported Seq-id found in CONTIG/CO line at location: \"{}\". Entry skipped.", location);
        else
            FtaErrPost(SEV_REJECT, ERR_LOCATION_SeqIdProblem, "Empty or unsupported Seq-id found in feature \"{}\" at location \"{}\". Entry skipped.", name, location);
        ibp->drop = true;
        return;
    }

    const CTextseq_id* text_id = id.GetTextseq_Id();
    if (! text_id || ! text_id->IsSetAccession()) {
        if (name.empty())
            FtaErrPost(SEV_REJECT, ERR_LOCATION_SeqIdProblem, "Empty Seq-id found in CONTIG/CO line at location: \"{}\". Entry skipped.", location);
        else
            FtaErrPost(SEV_REJECT, ERR_LOCATION_SeqIdProblem, "Empty Seq-id found in feature \"{}\" at location \"{}\". Entry skipped.", name, location);
        ibp->drop = true;
        return;
    }

    const Char* accession = text_id->GetAccession().c_str();
    if (iscon) {
        i = IsNewAccessFormat(accession);
        if (i == 3) {
            if (! slip->wgscont)
                slip->wgscont = accession;
            else if (! slip->wgsacc && ! StringEquN(slip->wgscont, accession, 4))
                slip->wgsacc = accession;
        } else if (i == 7) {
            if (! slip->wgsscaf)
                slip->wgsscaf = accession;
            else if (! slip->wgsacc && ! StringEquN(slip->wgsscaf, accession, 4))
                slip->wgsacc = accession;
        }
    }

    if (auto type = CSeq_id::GetAccType(CSeq_id::IdentifyAccession(text_id->GetAccession()));
        isSupportedAccession(type)) {
        if (type != id.Which()) {
            CRef<CTextseq_id> new_text_id(new CTextseq_id);
            new_text_id->Assign(*text_id);
            SetTextId(type, id, *new_text_id);
        }
    } else if (source == Parser::ESource::Flybase) {
        id.SetGeneral().SetDb("FlyBase");
        id.SetGeneral().SetTag().SetStr(accession);
    } else if (source == Parser::ESource::USPTO) {
        CRef<CPatent_seq_id> pat_id = MakeUsptoPatSeqId(accession);
        id.SetPatent(*pat_id);
    } else {
        if (name.empty())
            FtaErrPost(SEV_REJECT, ERR_LOCATION_SeqIdProblem, "Invalid accession found in CONTIG/CO line at location: \"{}\". Entry skipped.", location);
        else
            FtaErrPost(SEV_REJECT, ERR_LOCATION_SeqIdProblem, "Invalid accession found in feature \"{}\" at location \"{}\". Entry skipped.", name, location);
        ibp->drop = true;
        return;
    }

    slip->total++;

    if (id.IsGenbank()) {
        if (source != Parser::ESource::NCBI && source != Parser::ESource::All &&
            source != Parser::ESource::LANL && ! slip->badslp)
            slip->badslp = &loc;
        slip->genbank = 1;
    } else if (id.IsEmbl()) {
        if (source != Parser::ESource::EMBL && source != Parser::ESource::All &&
            ! slip->badslp)
            slip->badslp = &loc;
        slip->embl = 1;
    } else if (id.IsPir()) {
        if (source != Parser::ESource::All &&
            ! slip->badslp)
            slip->badslp = &loc;
        slip->pir = 1;
    } else if (id.IsSwissprot()) {
        if (source != Parser::ESource::SPROT && source != Parser::ESource::All &&
            ! slip->badslp)
            slip->badslp = &loc;
        slip->swissprot = 1;
    } else if (id.IsOther()) {
        if (source != Parser::ESource::Refseq && source != Parser::ESource::All &&
            ! slip->badslp)
            slip->badslp = &loc;
        slip->other = 1;
    } else if (id.IsDdbj()) {
        if (source != Parser::ESource::DDBJ && source != Parser::ESource::All &&
            ! slip->badslp)
            slip->badslp = &loc;
        slip->ddbj = 1;
    } else if (id.IsPrf()) {
        if (source != Parser::ESource::All &&
            ! slip->badslp)
            slip->badslp = &loc;
        slip->prf = 1;
    } else if (id.IsTpg()) {
        if (source != Parser::ESource::NCBI && source != Parser::ESource::All &&
            source != Parser::ESource::LANL && ! slip->badslp)
            slip->badslp = &loc;
        slip->tpg = 1;
    } else if (id.IsTpe()) {
        if (source != Parser::ESource::EMBL && source != Parser::ESource::All &&
            ! slip->badslp)
            slip->badslp = &loc;
        slip->tpe = 1;
    } else if (id.IsTpd()) {
        if (source != Parser::ESource::DDBJ && source != Parser::ESource::All &&
            ! slip->badslp)
            slip->badslp = &loc;
        slip->tpd = 1;
    }
}

/**********************************************************/
static void fta_do_fix_seq_loc_id(TSeqLocList& locs, IndexblkPtr ibp, string_view location, string_view name, SeqLocIdsPtr slip, bool iscon, Parser::ESource source)
{
    for (auto& loc : locs) {
        if (loc->IsEmpty()) {
            fta_fix_seq_id(*loc, loc->SetEmpty(), ibp, location, name, slip, iscon, source);
        } else if (loc->IsWhole()) {
            fta_fix_seq_id(*loc, loc->SetWhole(), ibp, location, name, slip, iscon, source);
        } else if (loc->IsInt()) {
            fta_fix_seq_id(*loc, loc->SetInt().SetId(), ibp, location, name, slip, iscon, source);
        } else if (loc->IsPnt()) {
            fta_fix_seq_id(*loc, loc->SetPnt().SetId(), ibp, location, name, slip, iscon, source);
            if (iscon && ! loc->GetPnt().IsSetFuzz()) {
                int                 point = loc->GetPnt().GetPoint();
                CRef<CSeq_interval> interval(new CSeq_interval);
                interval->SetFrom(point);
                interval->SetTo(point);

                if (loc->GetPnt().IsSetStrand())
                    interval->SetStrand(loc->GetPnt().GetStrand());

                interval->SetId(loc->SetPnt().SetId());
                loc->SetInt(*interval);
            }
        } else if (loc->IsPacked_int()) {
            for (auto& interval : loc->SetPacked_int().Set()) {
                fta_fix_seq_id(*loc, interval->SetId(), ibp, location, name, slip, iscon, source);
            }
        } else if (loc->IsPacked_pnt()) {
            fta_fix_seq_id(*loc, loc->SetPacked_pnt().SetId(), ibp, location, name, slip, iscon, source);
        } else if (loc->IsMix()) {
            fta_do_fix_seq_loc_id(loc->SetMix().Set(), ibp, location, name, slip, iscon, source);
        } else if (loc->IsEquiv()) {
            fta_do_fix_seq_loc_id(loc->SetEquiv().Set(), ibp, location, name, slip, iscon, source);
        }
    }
}

/**********************************************************/
Int4 fta_fix_seq_loc_id(TSeqLocList& locs, ParserPtr pp, string_view location, string_view name, bool iscon)
{
    SeqLocIds   sli;
    const Char* p = nullptr;
    ErrSev      sev;
    IndexblkPtr ibp;
    Int4        tpa;
    Int4        non_tpa;
    Int4        i = 0;

    ibp = pp->entrylist[pp->curindx];

    fta_do_fix_seq_loc_id(locs, ibp, location, name, &sli, iscon, pp->source);

    tpa     = sli.tpg + sli.tpe + sli.tpd;
    non_tpa = sli.genbank + sli.embl + sli.pir + sli.swissprot + sli.other +
              sli.ddbj + sli.prf;

    if (iscon && ! sli.wgsacc && sli.wgscont &&
        sli.wgsscaf && ! StringEquN(sli.wgscont, sli.wgsscaf, 4))
        sli.wgsacc = sli.wgsscaf;

    if ((tpa > 0 && non_tpa > 0) || tpa > 1 || non_tpa > 1 ||
        (iscon && sli.wgscont && sli.wgsscaf)) {
    }

    if (tpa > 0 && non_tpa > 0) {
        if (name.empty())
            FtaErrPost(SEV_REJECT, ERR_LOCATION_TpaAndNonTpa, "The CONTIG/CO line with location \"{}\" refers to intervals on both primary and third-party sequence records. Entry skipped.", location);
        else
            FtaErrPost(SEV_REJECT, ERR_LOCATION_TpaAndNonTpa, "The \"{}\" feature at \"{}\" refers to intervals on both primary and third-party sequence records. Entry skipped.", name, location);
        ibp->drop = true;
    }

    if (tpa > 1 || non_tpa > 1) {
        if (! pp->allow_crossdb_featloc) {
            sev       = SEV_REJECT;
            p         = "Entry skipped.";
            ibp->drop = true;
        } else {
            sev = SEV_WARNING;
            p   = "";
        }
        if (name.empty()) {
            string label;
            if (sli.badslp)
                sli.badslp->GetLabel(&label);

            FtaErrPost(sev, ERR_LOCATION_CrossDatabaseFeatLoc, "The CONTIG/CO line refers to intervals on records from two or more INSDC databases. This is not allowed without review and approval : \"{}\".{}", label.empty() ? location : label, p);
        } else
            FtaErrPost(sev, ERR_LOCATION_CrossDatabaseFeatLoc, "The \"{}\" feature at \"{}\" refers to intervals on records from two or more INSDC databases. This is not allowed without review and approval.{}", name, location, p);
    }

    if (iscon) {
        if (sli.wgscont && sli.wgsscaf)
            FtaErrPost(SEV_ERROR, ERR_LOCATION_ContigAndScaffold, "The CONTIG/CO line with location \"{}\" refers to intervals on both WGS contig and WGS scaffold records.", location);

        if (sli.wgsacc) {
            if (sli.wgscont && ! StringEquN(sli.wgscont, sli.wgsacc, 4))
                p = sli.wgscont;
            else if (sli.wgsscaf && ! StringEquN(sli.wgsscaf, sli.wgsacc, 4))
                p = sli.wgsscaf;

            if (p) {
                Char msga[5],
                    msgb[5];

                StringNCpy(msga, sli.wgsacc, 4);
                StringNCpy(msgb, p, 4);
                msga[4] = msgb[4] = 0;

                FtaErrPost(SEV_WARNING, ERR_SEQUENCE_MultipleWGSProjects, "This CON/scaffold record is assembled from the contigs of multiple WGS projects. First pair of WGS project codes is \"{}\" and \"{}\".", msgb, msga);
            }
        }

        i = IsNewAccessFormat(ibp->acnum);
        if (i == 3 || i == 7) {
            p = nullptr;
            if (sli.wgscont && ! StringEquN(sli.wgscont, ibp->acnum, 4))
                p = sli.wgscont;
            else if (sli.wgsscaf && ! StringEquN(sli.wgsscaf, ibp->acnum, 4))
                p = sli.wgsscaf;
            else if (sli.wgsacc && ! StringEquN(sli.wgsacc, ibp->acnum, 4))
                p = sli.wgsscaf; // ?

            if (p) {
                Char msg[5];
                StringNCpy(msg, p, 4);
                msg[4] = 0;

                FtaErrPost(SEV_WARNING, ERR_ACCESSION_WGSPrefixMismatch, "This WGS CON/scaffold record is assembled from the contigs of different WGS projects. First differing WGS project code is \"{}\".", msg);
            }
        }
    }

    if (sli.wgscont)
        sli.wgscont = nullptr;
    if (sli.wgsscaf)
        sli.wgsscaf = nullptr;
    if (sli.wgsacc)
        sli.wgsacc = nullptr;

    return (sli.total);
}

/**********************************************************/
static ValNodePtr fta_vnp_structured_comment(char* buf)
{
    ValNodePtr res;
    ValNodePtr vnp;
    char*      start;
    char*      p;
    char*      q;
    char*      r;
    bool       bad;

    if (! buf || *buf == '\0')
        return nullptr;

    for (p = buf; *p != '\0'; p++) {
        if (*p != '~')
            continue;

        for (p++; *p == ' ' || *p == '~'; p++)
            *p = ' ';
        p--;
    }

    bad = false;
    res = ValNodeNew(nullptr);
    vnp = res;
    for (start = buf;;) {
        p = StringStr(start, "::");
        if (! p) {
            if (start == buf)
                bad = true;
            break;
        }

        q = StringStr(p + 2, "::");
        if (! q) {
            vnp = ValNodeNew(vnp, start);
            for (r = vnp->data; *r != '\0'; r++)
                if (*r == '~')
                    *r = ' ';
            ShrinkSpaces(vnp->data);
            break;
        }

        *q = '\0';
        r  = StringRChr(p + 2, '~');
        *q = ':';
        if (! r) {
            bad = true;
            break;
        }

        *r  = '\0';
        vnp = ValNodeNew(vnp, start);
        *r  = '~';
        for (p = vnp->data; *p != '\0'; p++)
            if (*p == '~')
                *p = ' ';
        ShrinkSpaces(vnp->data);

        start = r;
    }

    vnp       = res->next;
    res->next = nullptr;
    ValNodeFree(res);

    if (! bad)
        return (vnp);

    ValNodeFreeData(vnp);
    return nullptr;
}

/**********************************************************/
static CRef<CUser_object> fta_build_structured_comment(char* tag, char* buf)
{
    ValNodePtr vnp;
    ValNodePtr tvnp;

    char* p;
    char* q;

    CRef<CUser_object> obj;

    if (! tag || *tag == '\0' || ! buf || *buf == '\0')
        return obj;

    vnp = fta_vnp_structured_comment(buf);
    if (! vnp)
        return obj;

    obj.Reset(new CUser_object);

    CObject_id& id = obj->SetType();
    id.SetStr("StructuredComment");

    CRef<CUser_field> field(new CUser_field);
    field->SetLabel().SetStr("StructuredCommentPrefix");

    field->SetData().SetStr() = tag;
    field->SetData().SetStr() += "-START##";

    obj->SetData().push_back(field);

    for (tvnp = vnp; tvnp; tvnp = tvnp->next) {
        p = tvnp->data;
        if (! p || *p == '\0')
            continue;

        q = StringStr(p, "::");
        if (! q)
            continue;

        if (q > p && *(q - 1) == ' ')
            q--;

        for (*q++ = '\0'; *q == ' ' || *q == ':';)
            q++;

        if (*p == '\0' || *q == '\0')
            continue;

        field.Reset(new CUser_field);
        field->SetLabel().SetStr(p);
        field->SetData().SetStr(q);

        obj->SetData().push_back(field);
    }

    if (obj->GetData().size() < 2) {
        obj.Reset();
        return obj;
    }

    field.Reset(new CUser_field);
    field->SetLabel().SetStr("StructuredCommentSuffix");
    field->SetData().SetStr() = tag;
    field->SetData().SetStr() += "-END##";

    obj->SetData().push_back(field);

    ValNodeFreeData(vnp);

    return obj;
}

/**********************************************************/
void fta_parse_structured_comment(char* str, bool& bad, TUserObjVector& objs)
{
    ValNodePtr tagvnp;
    ValNodePtr vnp;

    char* start;
    char* tag = nullptr;
    char* buf;
    char* p;
    char* q;
    char* r;

    if (! str || *str == '\0')
        return;

    tagvnp = nullptr;
    for (p = str;;) {
        p = StringStr(p, "-START##");
        if (! p)
            break;
        for (q = p;; q--)
            if (*q == '~' || (*q == '#' && q > str && *--q == '#') || q == str)
                break;
        if (q[0] != '#' || q[1] != '#') {
            p += 8;
            continue;
        }

        start = q;

        tag = StringSave(string_view(q, p - q));

        for (q = p;;) {
            q = StringStr(q, tag);
            if (! q) {
                bad = true;
                break;
            }
            size_t i = StringLen(tag);
            if (! StringEquN(q + i, "-END##", 6)) {
                q += (i + 6);
                continue;
            }
            r = StringStr(p + 8, "-START##");
            if (r && r < q) {
                bad = true;
                break;
            }
            break;
        }

        if (bad)
            break;

        if (! tagvnp) {
            tagvnp = ValNodeNew(nullptr, tag);
        } else {
            for (vnp = tagvnp; vnp; vnp = vnp->next) {
                r = vnp->data;
                if (StringEqu(r + 2, tag + 2)) {
                    if (*r != ' ') {
                        FtaErrPost(SEV_ERROR, ERR_COMMENT_SameStructuredCommentTags, "More than one structured comment with the same tag \"{}\" found.", tag + 2);
                        *r = ' ';
                    }
                    break;
                }
                if (! vnp->next) {
                    ValNodeNew(vnp, tag);
                    break;
                }
            }
        }

        if (StringEqu(tag, "##Metadata")) {
            MemFree(tag);
            p += 8;
            continue;
        }

        if (! SrchTheStr(p + 8, q, "::")) {
            FtaErrPost(SEV_ERROR, ERR_COMMENT_StructuredCommentLacksDelim, "The structured comment in this record lacks the expected double-colon '::' delimiter between fields and values.");
            MemFree(tag);
            p += 8;
            continue;
        }

        buf = StringSave(string(p + 8, q));

        CRef<CUser_object> cur = fta_build_structured_comment(tag, buf);
        MemFree(buf);

        if (cur.Empty()) {
            bad = true;
            break;
        }

        objs.push_back(cur);

        fta_StringCpy(start, q + StringLen(tag) + 6);
        MemFree(tag);
        p = start;
    }

    if (bad) {
        FtaErrPost(SEV_REJECT, ERR_COMMENT_InvalidStructuredComment, "Incorrectly formatted structured comment with tag \"{}\" encountered. Entry dropped.", tag + 2);
        MemFree(tag);
    }

    if (tagvnp)
        ValNodeFreeData(tagvnp);
}

/**********************************************************/
string GetQSFromFile(FILE* fd, const Indexblk* ibp)
{
    string ret;
    Char   buf[1024];

    if (! fd || ibp->qslength < 1)
        return ret;

    ret.reserve(ibp->qslength + 10);
    fseek(fd, static_cast<long>(ibp->qsoffset), 0);
    while (fgets(buf, 1023, fd)) {
        if (buf[0] == '>' && ret[0] != '\0')
            break;
        ret.append(buf);
    }
    return ret;
}

/**********************************************************/
void fta_remove_cleanup_user_object(CSeq_entry& seq_entry)
{
    TSeqdescList* descrs = nullptr;
    if (seq_entry.IsSeq()) {
        if (seq_entry.GetSeq().IsSetDescr())
            descrs = &seq_entry.SetSeq().SetDescr().Set();
    } else if (seq_entry.IsSet()) {
        if (seq_entry.GetSet().IsSetDescr())
            descrs = &seq_entry.SetSet().SetDescr().Set();
    }

    if (! descrs)
        return;

    for (TSeqdescList::iterator descr = descrs->begin(); descr != descrs->end();) {
        if (! (*descr)->IsUser()) {
            ++descr;
            continue;
        }

        const CUser_object& user_obj = (*descr)->GetUser();
        if (! user_obj.IsSetType() || ! user_obj.GetType().IsStr() ||
            user_obj.GetType().GetStr() != "NcbiCleanup") {
            ++descr;
            continue;
        }

        descr = descrs->erase(descr);
        break;
    }
}

/**********************************************************/
void fta_tsa_tls_comment_dblink_check(const CBioseq& bioseq,
                                      bool           is_tsa)
{
    bool got_comment = false;
    bool got_dblink  = false;

    for (const auto& descr : bioseq.GetDescr().Get()) {
        if (! descr->IsUser())
            continue;

        const CUser_object& user_obj = descr->GetUser();
        if (! user_obj.IsSetType() || ! user_obj.GetType().IsStr())
            continue;

        const string& user_type_str = user_obj.GetType().GetStr();

        if (user_type_str == "StructuredComment")
            got_comment = true;
        else if (user_type_str == "GenomeProjectsDB")
            got_dblink = true;
        else if (user_type_str == "DBLink") {
            for (const auto& field : user_obj.GetData()) {
                if (! field->IsSetLabel() || ! field->GetLabel().IsStr() ||
                    field->GetLabel().GetStr() != "BioProject")
                    continue;
                got_dblink = true;
                break;
            }
        }
    }

    if (! is_tsa) {
        if (! got_comment)
            FtaErrPost(SEV_WARNING, ERR_ENTRY_TLSLacksStructuredComment, "This TLS record lacks an expected structured comment.");
        if (! got_dblink)
            FtaErrPost(SEV_WARNING, ERR_ENTRY_TLSLacksBioProjectLink, "This TLS record lacks an expected BioProject or Project link.");
    } else {
        if (! got_comment)
            FtaErrPost(SEV_WARNING, ERR_ENTRY_TSALacksStructuredComment, "This TSA record lacks an expected structured comment.");
        if (! got_dblink)
            FtaErrPost(SEV_WARNING, ERR_ENTRY_TSALacksBioProjectLink, "This TSA record lacks an expected BioProject or Project link.");
    }
}

/**********************************************************/
void fta_set_molinfo_completeness(CBioseq& bioseq, const Indexblk* ibp)
{
    if (bioseq.GetInst().GetTopology() != CSeq_inst::eTopology_circular || (ibp && ! ibp->gaps.empty()))
        return;

    CMolInfo* mol_info = nullptr;
    for (auto& descr : bioseq.SetDescr().Set()) {
        if (descr->IsMolinfo()) {
            mol_info = &descr->SetMolinfo();
            break;
        }
    }

    if (mol_info) {
        mol_info->SetCompleteness(CMolInfo::eCompleteness_complete);
    } else {
        CRef<CSeqdesc> descr(new CSeqdesc);
        CMolInfo&      mol = descr->SetMolinfo();
        mol.SetCompleteness(CMolInfo::eCompleteness_complete);

        bioseq.SetDescr().Set().push_back(descr);
    }
}

/**********************************************************/
void fta_create_far_fetch_policy_user_object(CBioseq& bsp, Int4 num)
{
    if (num < 1000)
        return;

    FtaErrPost(SEV_INFO, ERR_SEQUENCE_HasManyComponents, "An OnlyNearFeatures FeatureFetchPolicy User-object has been added to this record because it is constructed from {} components, which exceeds the threshold of 999 for User-object creation.", num);

    CRef<CSeqdesc> descr(new CSeqdesc);
    descr->SetUser().SetType().SetStr("FeatureFetchPolicy");

    CRef<CUser_field> field(new CUser_field);

    field->SetLabel().SetStr("Policy");
    field->SetData().SetStr("OnlyNearFeatures");

    descr->SetUser().SetData().push_back(field);

    bsp.SetDescr().Set().push_back(descr);
}

/**********************************************************/
void StripECO(string& str)
{
    for (size_t i = str.find("{ECO:"); i != string::npos; i = str.find("{ECO:", i)) {
        size_t j = str.find('}', i);
        if (j == string::npos)
            break;
        ++j;
        if (i > 0 && str[i - 1] == ' ')
            --i;
        if (i > 0 && j < str.size()) {
            if ((str[i - 1] == '.' && str[j] == '.') ||
                (str[i - 1] == ';' && str[j] == ';')) {
                --i;
            }
        }
        str.erase(i, j - i);
    }
}

/**********************************************************/
bool fta_dblink_has_sra(const CRef<CUser_object>& uop)
{
    if (uop.Empty() || ! uop->IsSetData() || ! uop->IsSetType() ||
        ! uop->GetType().IsStr() || uop->GetType().GetStr() != "DBLink")
        return false;

    bool got = false;

    for (const auto& field : uop->GetData()) {
        if (! field->IsSetData() || ! field->GetData().IsStrs() || ! field->IsSetNum() || field->GetNum() < 1 ||
            ! field->IsSetLabel() || ! field->GetLabel().IsStr() || field->GetLabel().GetStr() != "Sequence Read Archive")
            continue;

        for (const CStringUTF8& str : field->GetData().GetStrs()) {
            if (str.size() > 2 &&
                (str[0] == 'D' || str[0] == 'E' || str[0] == 'S') && str[1] == 'R' &&
                (str[2] == 'R' || str[2] == 'X' || str[2] == 'Z')) {
                got = true;
                break;
            }
        }
        if (got)
            break;
    }
    return (got);
}

END_NCBI_SCOPE
