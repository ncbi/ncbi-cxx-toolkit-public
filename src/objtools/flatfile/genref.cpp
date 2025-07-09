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
 * File Name: genref.cpp
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 *      Parse gene qualifiers.
 *
 */

#include <ncbi_pch.hpp>

#include "ftacpp.hpp"

#include "index.h"

#include <objtools/flatfile/flatfile_parser.hpp>
#include <objtools/flatfile/flatdefn.h>

#include "ftaerr.hpp"
#include "utilfeat.h"
#include "add.h"
#include "nucprot.h"

#include <objmgr/bioseq_ci.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objects/seqloc/Seq_bond.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/RNA_ref.hpp>

#include "asci_blk.h"
#include "utilfun.h"

#ifdef THIS_FILE
#  undef THIS_FILE
#endif
#define THIS_FILE "genref.cpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

struct SeqLocInfo {
    CRef<CSeq_loc> loc;
    ENa_strand     strand;
};

typedef list<SeqLocInfo> TSeqLocInfoList;
typedef std::set<string> TSynSet;
typedef std::set<string> TWormbaseSet;
typedef std::set<string> TLocusTagSet;

struct AccMinMax {
    CRef<CSeq_id> pId;

    // string acc;
    // Int2   ver = INT2_MIN;
    Int4 min = -1;
    Int4 max = -1;
};


struct GeneLoc {
    string          gene;
    string          locus;
    Int4            strand;
    Int4            verymin;
    Int4            verymax;
    list<AccMinMax> ammp;
};
using TGeneLocList = forward_list<GeneLoc>;


struct SeqlocInfoblk {
    Int4       from   = 0; /* the lowest value in the entry */
    Int4       to     = 0; /* the highest value in the entry */
    ENa_strand strand = eNa_strand_unknown;
    Int4       length = 0; /* total length of the seq-data of the entry */
    TSeqIdList ids;        /* the entry's SeqId */
    bool       noleft  = false;
    bool       noright = false;
};

using SeqlocInfoblkPtr = SeqlocInfoblk*;

struct MixLoc {
    CRef<CSeq_id> pId;
    Int4          min     = 0;
    Int4          max     = 0;
    ENa_strand    strand  = eNa_strand_unknown;
    bool          noleft  = false;
    bool          noright = false;
    Int4          numint  = 0;
    MixLoc*       next    = nullptr;
};

using MixLocPtr = MixLoc*;

struct Gene {
    string locus; /* the name of the gene,
                                           copy the value */
    string locus_tag;
    string pseudogene;
    string maploc; /* the map of the gene,
                                           copy the value */

    CRef<CSeq_feat> feat;

    SeqlocInfoblkPtr slibp  = nullptr; /* the location, points to the value */
    Uint1            leave  = 0;       /* TRUE for aka tRNAs */

    CRef<CSeq_loc> loc;

    MixLocPtr mlp = nullptr;

    TSynSet      syn;
    TWormbaseSet wormbase;
    TLocusTagSet olt; /* /old_locus_tag values */

    bool   pseudo    = false;
    bool   allpseudo = false;
    bool   genefeat  = false;
    bool   noleft    = false;
    bool   noright   = false;
    string fname;
    string location;
    bool   todel    = false;
    bool   circular = false;
    Gene*  next     = nullptr;

    ~Gene();
};
using TGeneList = Gene*;
using GeneListPtr = Gene*;

struct Cds {
    Int4 from;
    Int4 to;
};
using TCdsList = forward_list<Cds>;

struct GeneNode {
    bool         flag; /* TRUE, if a level has been found
                                           to put GeneRefPtr */
    CBioseq*     bioseq;
    CBioseq_set* bioseq_set;

    TGeneList    gl = nullptr; /* a list of gene infomation for
                                           the entries */
    TSeqFeatList feats;    /* a list which contains the
                                           GeneRefPtr only */
    bool         accver;   /* for ACCESSION.VERSION */
    bool         skipdiv;  /* skip BCT and SYN divisions */
    TCdsList     cdsl;
    bool         circular;
    TGeneLocList gelocs;
    bool         simple_genes;
    bool         got_misc; /* TRUE if there is a misc_feature
                                           with gene or/and locus_tag */

    GeneNode() :
        flag(false),
        bioseq(nullptr),
        bioseq_set(nullptr),
        accver(false),
        skipdiv(false),
        circular(false),
        simple_genes(false),
        got_misc(false)
    {
    }
};

using GeneNodePtr = GeneNode*;

/* The list of feature which are not allowed to have gene qual
 */
const char* feat_no_gene[] = { "gap", "operon", "source", nullptr };

const char* leave_imp_feat[] = {
    "LTR",
    "conflict",
    "rep_origin",
    "repeat_region",
    "satellite",
    nullptr
};

Int2 leave_rna_feat[] = {
    3, /* tRNA */
    4, /* rRNA */
    5, /* snRNA */
    6, /* scRNA */
    -1
};

/**********************************************************/
static void GetLocationStr(const CSeq_loc& loc, string& str)
{
    loc.GetLabel(&str);
    MakeLocStrCompatible(str);
}

/**********************************************************/
static bool fta_seqid_same(const CSeq_id& sid, const Char* acnum, const CSeq_id* id)
{
    if (id)
        return sid.Compare(*id) == CSeq_id::e_YES;

    if (! acnum)
        return true;

    auto id_string = sid.GetSeqIdString();
    return NStr::EqualCase(id_string, acnum);
    /*
    if (! sid.IsGenbank() && ! sid.IsEmbl() && ! sid.IsDdbj() &&
        ! sid.IsPir() && ! sid.IsSwissprot() && ! sid.IsOther() &&
        ! sid.IsPrf() && ! sid.IsTpg() && ! sid.IsTpd() &&
        ! sid.IsTpe() && ! sid.IsGpipe())
        return false;

    const CTextseq_id* text_id = sid.GetTextseq_Id();
    if (! text_id || ! text_id->IsSetAccession() ||
        text_id->GetAccession() != acnum)
        return false;

    return true;
    */
}

/**********************************************************/
static void fta_seqloc_del_far(CSeq_loc& locs, const Char* acnum, const CSeq_id* id)
{
    vector<CConstRef<CSeq_loc>> to_remove;

    for (CSeq_loc_CI ci(locs, CSeq_loc_CI::eEmpty_Allow); ci != locs.end(); ++ci) {
        CConstRef<CSeq_loc> cur_loc = ci.GetRangeAsSeq_loc();
        if (cur_loc->IsWhole()) {
            if (fta_seqid_same(*cur_loc->GetId(), acnum, id))
                continue;
        } else if (cur_loc->IsInt()) {
            if (fta_seqid_same(cur_loc->GetInt().GetId(), acnum, id))
                continue;
        } else if (cur_loc->IsPnt()) {
            if (fta_seqid_same(cur_loc->GetPnt().GetId(), acnum, id))
                continue;
        } else if (cur_loc->IsBond()) {
            if (cur_loc->GetBond().IsSetA() &&
                fta_seqid_same(cur_loc->GetBond().GetA().GetId(), acnum, id))
                continue;
        } else if (cur_loc->IsPacked_pnt()) {
            if (fta_seqid_same(cur_loc->GetPacked_pnt().GetId(), acnum, id))
                continue;
        }

        to_remove.push_back(cur_loc);
    }

    for (vector<CConstRef<CSeq_loc>>::const_iterator it = to_remove.begin(); it != to_remove.end(); ++it)
        locs.Assign(*locs.Subtract(*(*it), 0, nullptr, nullptr));

    for (CTypeIterator<CSeq_bond> bond(locs); bond; ++bond) {
        if (bond->IsSetB() && ! fta_seqid_same(bond->GetB().GetId(), acnum, id))
            bond->ResetB();
    }
}


/**********************************************************/
static CRef<CSeq_loc> fta_seqloc_local(const CSeq_loc& orig, const Char* acnum)
{
    CRef<CSeq_loc> ret(new CSeq_loc);
    ret->Assign(orig);

    if (acnum && *acnum != '\0' && *acnum != ' ')
        fta_seqloc_del_far(*ret, acnum, nullptr);

    return ret;
}

/**********************************************************/
static Int4 fta_cmp_gene_syns(const TSynSet& syn1, const TSynSet& syn2)
{
    Int4 i = 0;

    TSynSet::const_iterator it1 = syn1.begin(),
                            it2 = syn2.begin();

    for (; it1 != syn1.end() && it2 != syn2.end(); ++it1, ++it2) {
        i = NStr::CompareNocase(*it1, *it2);
        if (i != 0)
            break;
    }

    if (it1 == syn1.end() && it2 == syn2.end())
        return (0);
    if (it1 == syn1.end())
        return (-1);
    if (it2 == syn2.end())
        return (1);
    return (i);
}


/**********************************************************/
static Int4 fta_cmp_gene_refs(const CGene_ref& grp1, const CGene_ref& grp2)
{
    Int4 res = 0;

    TSynSet syn1,
        syn2;

    if (grp1.IsSetSyn())
        syn1.insert(grp1.GetSyn().begin(), grp1.GetSyn().end());
    if (grp2.IsSetSyn())
        syn2.insert(grp2.GetSyn().begin(), grp2.GetSyn().end());

    if (! grp1.IsSetLocus() && ! grp2.IsSetLocus()) {
        res = fta_cmp_gene_syns(syn1, syn2);
        if (res != 0)
            return (res);
        return (NStr::CompareNocase(grp1.IsSetLocus_tag() ? grp1.GetLocus_tag() : "",
                                    grp2.IsSetLocus_tag() ? grp2.GetLocus_tag() : ""));
    }

    if (! grp1.IsSetLocus())
        return (-1);
    if (! grp2.IsSetLocus())
        return (1);

    res = NStr::CompareNocase(grp1.GetLocus(), grp2.GetLocus());
    if (res != 0)
        return (res);

    res = fta_cmp_gene_syns(syn1, syn2);
    if (res != 0)
        return (res);

    return (NStr::CompareNocase(grp1.IsSetLocus_tag() ? grp1.GetLocus_tag() : "",
                                grp2.IsSetLocus_tag() ? grp2.GetLocus_tag() : ""));
}

/**********************************************************/
static Int4 fta_cmp_locusyn(const Gene& g1, const Gene& g2)
{
    Int4 res;

    if (g1.locus.empty() && g2.locus.empty()) {
        res = fta_cmp_gene_syns(g1.syn, g2.syn);
        if (res != 0)
            return (res);
        return (NStr::CompareNocase(g1.locus_tag, g2.locus_tag));
    }
    if (g1.locus.empty())
        return (-1);
    if (g2.locus.empty())
        return (1);

    res = NStr::CompareNocase(g1.locus, g2.locus);
    if (res != 0)
        return (res);
    res = fta_cmp_gene_syns(g1.syn, g2.syn);
    if (res != 0)
        return (res);
    return (NStr::CompareNocase(g1.locus_tag, g2.locus_tag));
}

/**********************************************************/
static bool CompareGeneListName(const Gene* sp1, const Gene* sp2)
{
    SeqlocInfoblk* slip1 = sp1->slibp;
    SeqlocInfoblk* slip2 = sp2->slibp;

    Int4 status = fta_cmp_locusyn(*sp1, *sp2);
    if (status != 0)
        return status < 0;

    status = slip1->strand - slip2->strand;
    if (status != 0)
        return status < 0;

    status = (Int4)(sp1->leave - sp2->leave);
    if (status != 0)
        return status < 0;

    status = slip1->from - slip2->from;
    if (status != 0)
        return status < 0;

    status = slip2->noleft - slip1->noleft;
    if (status != 0)
        return status < 0;

    status = slip1->to - slip2->to;
    if (status != 0)
        return status < 0;

    return slip1->noright < slip2->noright;
}

/**********************************************************/
static void sort_gnp_list(TGeneList& gl)
{
    Int4 index;
    Int4 total;

    Gene* glp;

    total = 0;
    for (glp = gl; glp; glp = glp->next)
        total++;

    vector<Gene*> temp(total);

    for (index = 0, glp = gl; glp; glp = glp->next)
        temp[index++] = glp;

    std::sort(temp.begin(), temp.end(), CompareGeneListName);

    gl = glp = temp[0];
    for (index = 0; index < total - 1; glp = glp->next, index++)
        glp->next = temp[index + 1];

    glp       = temp[total - 1];
    glp->next = nullptr;
}


/**********************************************************/
static void MixLocFree(MixLocPtr mlp)
{
    MixLocPtr next;

    for (; mlp; mlp = next) {
        next = mlp->next;
        delete mlp;
    }
}

/**********************************************************/
Gene::~Gene()
{
    if (slibp)
        delete slibp;
    if (mlp)
        MixLocFree(mlp);
}
static void GeneListFree(TGeneList& gl)
{
    for (auto glp = gl; glp;) {
        auto glpnext = glp->next;
        glp->next    = nullptr;
        delete glp;
        glp = glpnext;
    }
    gl = nullptr;
}

/**********************************************************
 *
 *   GetLowHighFromSeqLoc:
 *   -- get the lowest "from", highest "to" value, strand
 *      value within one feature key
 *
 **********************************************************/
static SeqlocInfoblkPtr GetLowHighFromSeqLoc(const CSeq_loc* origslp, Int4 length, const CSeq_id& orig_id)
{
    SeqlocInfoblkPtr slibp = nullptr;

    Int4 from;
    Int4 to;

    ENa_strand strand;

    bool noleft;
    bool noright;

    if (origslp) {
        for (CSeq_loc_CI loc(*origslp); loc; ++loc) {
            noleft  = false;
            noright = false;

            CConstRef<CSeq_loc> cur_loc = loc.GetRangeAsSeq_loc();
            const CSeq_id*      id      = nullptr;

            if (cur_loc->IsInt()) {
                const CSeq_interval& interval = cur_loc->GetInt();
                id                            = &interval.GetId();

                from   = interval.GetFrom();
                to     = interval.GetTo();
                strand = interval.IsSetStrand() ? interval.GetStrand() : eNa_strand_unknown;

                if (interval.IsSetFuzz_from() && interval.GetFuzz_from().IsLim() &&
                    interval.GetFuzz_from().GetLim() == CInt_fuzz::eLim_lt)
                    noleft = true;
                if (interval.IsSetFuzz_to() && interval.GetFuzz_to().IsLim() &&
                    interval.GetFuzz_to().GetLim() == CInt_fuzz::eLim_gt)
                    noright = true;
            } else if (cur_loc->IsPnt()) {
                const CSeq_point& point = cur_loc->GetPnt();
                id                      = &point.GetId();
                from                    = point.GetPoint();
                to                      = from;
                strand                  = point.IsSetStrand() ? point.GetStrand() : eNa_strand_unknown;

                if (point.IsSetFuzz() && point.GetFuzz().IsLim()) {
                    if (point.GetFuzz().GetLim() == CInt_fuzz::eLim_gt)
                        noright = true;
                    else if (point.GetFuzz().GetLim() == CInt_fuzz::eLim_lt)
                        noleft = true;
                }
            } else
                continue;

            /* get low and high only for locations from the same entry */
            if (from < 0 || to < 0 || ! id || orig_id.Compare(*id) == CSeq_id::e_NO)
                continue;

            if (! slibp) {
                slibp          = new SeqlocInfoblk;
                slibp->from    = from;
                slibp->to      = to;
                slibp->noleft  = noleft;
                slibp->noright = noright;
                if (length != -99) {
                    slibp->strand = strand;

                    CRef<CSeq_id> sid(new CSeq_id);
                    sid->Assign(*id);
                    slibp->ids.push_back(sid);
                    slibp->length = length; /* total bsp of the entry */
                }
            } else {
                if (slibp->from > from) {
                    slibp->from   = from;
                    slibp->noleft = noleft;
                }
                if (slibp->to < to) {
                    slibp->to      = to;
                    slibp->noright = noright;
                }
            }
        }
    }

    return (slibp);
}

/**********************************************************/
static bool DoWeHaveGeneInBetween(Gene& g, SeqlocInfoblkPtr second, const GeneNode* gnp)
{
    SeqlocInfoblkPtr slp;
    SeqlocInfoblkPtr first;

    first = g.slibp;
    if (! gnp || first->to >= second->from)
        return false;

    auto glp = gnp->gl;
    for (; glp; glp = glp->next) {
        if (! glp->slibp)
            continue;

        slp = glp->slibp;
        if (gnp->skipdiv == false) {
            if (glp->leave != 0 || slp->strand != first->strand ||
                slp->from < first->from || slp->to > second->to ||
                fta_cmp_locusyn(*glp, g) == 0)
                continue;
            break;
        }

        if (slp->to <= first->to || slp->from >= second->from ||
            slp->strand != first->strand || fta_cmp_locusyn(*glp, g) == 0)
            continue;
        break;
    }

    return glp != nullptr;
}

/**********************************************************/
static bool DoWeHaveCdssInBetween(const Gene& g, Int4 to, const TCdsList& cdsl)
{
    const SeqlocInfoblk* cloc = g.slibp;
    if (cloc->to >= to)
        return false;
    for (const auto& clp : cdsl)
        if (clp.from > cloc->to && clp.to < to)
            return true;

    return false;
}

/**********************************************************/
static void AddGeneFeat(Gene& g, const string& maploc, TSeqFeatList& feats)
{
    CRef<CSeq_feat> feat(new CSeq_feat);
    CGene_ref&      gene_ref = feat->SetData().SetGene();

    if (! g.locus.empty()) {
        gene_ref.SetLocus(g.locus);
    }
    if (! g.locus_tag.empty()) {
        gene_ref.SetLocus_tag(g.locus_tag);
    }
    if (! maploc.empty()) {
        gene_ref.SetMaploc(maploc);
    }

    if (! g.syn.empty()) {
        gene_ref.SetSyn().assign(g.syn.begin(), g.syn.end());
        g.syn.clear();
    }

    if (g.loc.NotEmpty())
        feat->SetLocation(*g.loc);

    if (g.pseudo)
        feat->SetPseudo(true);

    if (g.allpseudo)
        feat->SetPseudo(true);

    if (! g.pseudogene.empty()) {
        CRef<CGb_qual> qual(new CGb_qual);
        qual->SetQual("pseudogene");
        qual->SetVal(g.pseudogene);

        feat->SetQual().push_back(qual);
        feat->SetPseudo(true);
    }

    if (! g.wormbase.empty()) {
        if (g.wormbase.size() > 1)
            FtaErrPost(SEV_WARNING, ERR_FEATURE_MultipleWBGeneXrefs, "Multiple WormBase WBGene /db_xref qualifiers found for feature with Gene Symbol \"{}\" and Locus Tag \"{}\".", (g.locus.empty()) ? "NONE" : g.locus, (g.locus_tag.empty()) ? "NONE" : g.locus_tag);

        for (TWormbaseSet::const_iterator it = g.wormbase.begin(); it != g.wormbase.end(); ++it) {
            if (it->empty())
                continue;

            CRef<CDbtag> tag(new CDbtag);

            tag->SetDb("WormBase");
            tag->SetTag().SetStr(*it);

            feat->SetDbxref().push_back(tag);
        }
    }

    if (! g.olt.empty()) {
        if (g.olt.size() > 1)
            FtaErrPost(SEV_WARNING, ERR_FEATURE_MultipleOldLocusTags, "Multiple /old_locus_tag qualifiers found for feature with Gene Symbol \"{}\" and Locus Tag \"{}\".", (g.locus.empty()) ? "NONE" : g.locus, (g.locus_tag.empty()) ? "NONE" : g.locus_tag);

        for (TLocusTagSet::const_iterator it = g.olt.begin(); it != g.olt.end(); ++it) {
            if (it->empty())
                continue;

            CRef<CGb_qual> qual(new CGb_qual);
            qual->SetQual("old_locus_tag");
            qual->SetVal(*it);

            feat->SetQual().push_back(qual);
        }
    }

    g.loc.Reset();
    feats.push_back(feat);
}

/**********************************************************/
static MixLocPtr MixLocCopy(const MixLoc& ml)
{
    MixLocPtr res = new MixLoc();
    res->pId      = ml.pId;
    res->min      = ml.min;
    res->max      = ml.max;
    res->strand   = ml.strand;
    res->noleft   = ml.noleft;
    res->noright  = ml.noright;
    res->numint   = ml.numint;
    return res;
}
/**********************************************************/

static bool s_IdsMatch(const CRef<CSeq_id>& pId1, const CRef<CSeq_id>& pId2)
{
    if (! pId1 && ! pId2) {
        return true;
    }

    if (! pId1 || ! pId2) {
        return false;
    }

    return (pId1->Compare(*pId2) == CSeq_id::e_YES);
}

/**********************************************************/
static MixLocPtr EasySeqLocMerge(MixLocPtr first, MixLocPtr second, bool join)
{
    MixLocPtr mlp;
    MixLocPtr res;
    MixLocPtr tres;
    MixLocPtr next;
    MixLocPtr prev;
    MixLocPtr ttt;
    Int2      got;

    if (! first && ! second)
        return nullptr;

    tres = new MixLoc;
    res  = tres;
    mlp  = first ? first : second;
    for (; mlp; mlp = mlp->next) {
        res->next = MixLocCopy(*mlp);
        res       = res->next;
    }
    if (first && second) {
        for (mlp = second; mlp; mlp = mlp->next) {
            next = MixLocCopy(*mlp);
            for (res = tres->next; res; res = res->next) {
                if (! s_IdsMatch(res->pId, next->pId) ||
                    res->strand != next->strand)
                    continue;

                ttt        = res->next;
                res->next  = next;
                next->next = ttt;
                break;
            }
            if (res)
                continue;

            for (prev = tres; prev->next; prev = prev->next) {
                ttt = prev->next;
                for (res = mlp->next; res; res = res->next) {
                    if (s_IdsMatch(res->pId, ttt->pId) &&
                        res->strand == ttt->strand)
                        break;
                }
                if (res)
                    break;
            }
            ttt        = prev->next;
            prev->next = next;
            next->next = ttt;
        }
    }

    res = tres->next;
    delete tres;
    if (! res)
        return nullptr;

    for (got = 1; got == 1;) {
        got = 0;
        for (tres = res; tres; tres = tres->next) {
            if (! tres->pId)
                continue;
            for (mlp = tres->next; mlp; mlp = mlp->next) {
                if (! mlp->pId ||
                    ! s_IdsMatch(tres->pId, mlp->pId) ||
                    tres->strand != mlp->strand)
                    continue;

                if (tres->min == mlp->min && tres->max == mlp->max) {
                    mlp->pId.Reset();
                    if (tres->noleft == false)
                        tres->noleft = mlp->noleft;
                    if (tres->noright == false)
                        tres->noright = mlp->noright;
                    got = 1;
                    continue;
                }

                if (join == false ||
                    (tres->min <= mlp->max + 1 && tres->max + 1 >= mlp->min)) {
                    if (tres->min == mlp->min) {
                        if (tres->noleft == false)
                            tres->noleft = mlp->noleft;
                    } else if (tres->min > mlp->min) {
                        tres->min    = mlp->min;
                        tres->noleft = mlp->noleft;
                    }

                    if (tres->max == mlp->max) {
                        if (tres->noright == false)
                            tres->noright = mlp->noright;
                    } else if (tres->max < mlp->max) {
                        tres->max     = mlp->max;
                        tres->noright = mlp->noright;
                    }
                    mlp->pId.Reset();
                    got = 1;
                }
            }
        }
    }
    for (mlp = nullptr, tres = res; tres; tres = next) {
        next = tres->next;
        if (tres->pId) {
            mlp = tres;
            continue;
        }
        if (! mlp)
            res = tres->next;
        else
            mlp->next = tres->next;
        tres->next = nullptr;
        MixLocFree(tres);
    }
    return (res);
}

/**********************************************************/
static MixLocPtr CircularSeqLocCollect(MixLocPtr first, MixLocPtr second)
{
    if (! first && ! second)
        return nullptr;

    MixLocPtr tres = new MixLoc,
              res  = tres;
    for (MixLocPtr mlp = first; mlp; mlp = mlp->next) {
        res->next = MixLocCopy(*mlp);
        res       = res->next;
    }
    for (MixLocPtr mlp = second; mlp; mlp = mlp->next) {
        res->next = MixLocCopy(*mlp);
        res       = res->next;
    }

    res = tres->next;
    delete tres;
    return (res);
}

/**********************************************************/
static void fta_add_wormbase(Gene& fromg, Gene& tog)
{
    tog.wormbase.insert(fromg.wormbase.begin(), fromg.wormbase.end());
    fromg.wormbase.clear();
}

/**********************************************************/
static void fta_add_olt(Gene& fromg, Gene& tog)
{
    tog.olt.insert(fromg.olt.begin(), fromg.olt.end());
    fromg.olt.clear();
}

/**********************************************************/
static void fta_check_pseudogene(Gene& tg, Gene& g)
{
    if (tg.pseudogene.empty() && g.pseudogene.empty())
        return;

    if (! tg.pseudogene.empty() && ! g.pseudogene.empty()) {
        if (tg.pseudogene != g.pseudogene) {
            FtaErrPost(SEV_ERROR, ERR_FEATURE_InconsistentPseudogene, "All /pseudogene qualifiers for a given Gene and/or Locus-Tag should be uniform. But pseudogenes \"{}\" vs. \"{}\" exist for the features with Gene Symbol \"{}\" and Locus Tag \"{}\".", (g.locus.empty()) ? "NONE" : g.locus, (g.locus_tag.empty()) ? "NONE" : g.locus_tag, tg.pseudogene, g.pseudogene);
            tg.pseudogene.clear();
            g.pseudogene.clear();
        }
        return;
    }

    if (tg.pseudogene.empty()) {
        tg.pseudogene = g.pseudogene;
    } else if (g.pseudogene.empty()) {
        g.pseudogene = tg.pseudogene;
    }
}


/**********************************************************/
static bool fta_check_feat_overlap(const TGeneLocList& gelocs, Gene& g, MixLocPtr mlp, Int4 from, Int4 to)
{
    Int4 min;
    Int4 max;

    if (gelocs.empty() || ! mlp)
        return true;

    min = (mlp->min > from) ? from : mlp->min;
    max = (mlp->max < to) ? to : mlp->max;

    auto gelop = gelocs.begin();
    for (; gelop != gelocs.end(); ++gelop) {
        if (min > gelop->verymax) {
            return false;
        }

        if ((gelop->strand > -1 && g.slibp->strand != gelop->strand) ||
            max < gelop->verymin)
            continue;

        if (fta_strings_same(g.locus.c_str(), gelop->gene.c_str()) && fta_strings_same(g.locus_tag.c_str(), gelop->locus.c_str()))
            continue;
        auto it = gelop->ammp.begin();
        for (; it != gelop->ammp.end(); ++it) {
            auto   ammp = *it;
            int    ver1 = 0;
            string label1;
            ammp.pId->GetLabel(&label1, &ver1);
            string label2;
            int    ver2 = 0;
            mlp->pId->GetLabel(&label2, &ver2);
            if (max < ammp.min || min > ammp.max || ver1 != ver2)
                continue;
            if (label1 == label2)
                break;
        }
        if (it != gelop->ammp.end()) {
            break;
        }
    }

    return gelop != gelocs.end();
}

/**********************************************************/
static bool ConfirmCircular(MixLocPtr mlp)
{
    MixLocPtr tmlp;

    if (! mlp || ! mlp->next)
        return false;

    tmlp = mlp;
    if (mlp->strand != eNa_strand_minus) {
        for (; tmlp->next; tmlp = tmlp->next)
            if (tmlp->min > tmlp->next->min)
                break;
    } else {
        for (; tmlp->next; tmlp = tmlp->next)
            if (tmlp->min < tmlp->next->min)
                break;
    }

    if (tmlp->next)
        return true;

    for (tmlp = mlp; tmlp; tmlp = tmlp->next) {
        tmlp->numint = 0;
    }
    return false;
}

/**********************************************************/
static void FixMixLoc(Gene& g, const TGeneLocList& gelocs)
{
    Int4       from;
    Int4       to;
    MixLocPtr  mlp;
    MixLocPtr  tmlp;
    ENa_strand strand;

    bool noleft;
    bool noright;
    bool tempcirc;

    g.mlp = nullptr;

    if (g.feat.Empty() || ! g.feat->IsSetLocation()) {
        CRef<CSeq_id> pTempId;
        for (auto pId : g.slibp->ids) {
            if (pId) {
                pTempId = pId;
                break;
            }
        }

        if (! pTempId) {
            return;
        }

        mlp          = new MixLoc();
        mlp->pId     = pTempId;
        mlp->min     = g.slibp->from;
        mlp->max     = g.slibp->to;
        mlp->strand  = g.slibp->strand;
        mlp->noleft  = g.slibp->noleft;
        mlp->noright = g.slibp->noright;
        mlp->numint  = 0;
        g.mlp        = mlp;
        return;
    }

    if (g.leave == 1) {
        g.loc.Reset(&g.feat->SetLocation());
        return;
    }

    mlp                  = nullptr;
    Int4            i    = 1;
    const CSeq_loc& locs = g.feat->GetLocation();
    for (CSeq_loc::const_iterator loc = locs.begin(); loc != locs.end(); ++loc) {
        noleft  = false;
        noright = false;
        CRef<CSeq_id> pId;

        CConstRef<CSeq_loc> cur_loc = loc.GetRangeAsSeq_loc();
        if (cur_loc->IsInt()) {
            const CSeq_interval& interval = cur_loc->GetInt();
            if (interval.IsSetId()) {
                pId = Ref(new CSeq_id());
                pId->Assign(interval.GetId());
            }

            from   = interval.IsSetFrom() ? interval.GetFrom() : 0;
            to     = interval.IsSetTo() ? interval.GetTo() : 0;
            strand = cur_loc->IsSetStrand() ? cur_loc->GetStrand() : eNa_strand_unknown;

            if (interval.IsSetFuzz_from() && interval.GetFuzz_from().IsLim() && interval.GetFuzz_from().GetLim() == CInt_fuzz::eLim_lt)
                noleft = true;

            if (interval.IsSetFuzz_to() && interval.GetFuzz_to().IsLim() && interval.GetFuzz_to().GetLim() == CInt_fuzz::eLim_gt)
                noright = true;
        } else if (cur_loc->IsPnt()) {
            const CSeq_point& point = cur_loc->GetPnt();
            if (point.IsSetId()) {
                pId = Ref(new CSeq_id());
                pId->Assign(point.GetId());
            }

            from   = point.IsSetPoint() ? point.GetPoint() : 0;
            to     = from;
            strand = cur_loc->IsSetStrand() ? cur_loc->GetStrand() : eNa_strand_unknown;

            if (point.IsSetFuzz() && point.GetFuzz().IsLim()) {
                if (point.GetFuzz().GetLim() == CInt_fuzz::eLim_gt)
                    noright = true;
                else if (point.GetFuzz().GetLim() == CInt_fuzz::eLim_lt)
                    noleft = true;
            }
        } else
            continue;

        if (! pId || from < 0 || to < 0) {
            continue;
        }


        if (! mlp) {
            mlp          = new MixLoc();
            mlp->pId     = pId;
            mlp->min     = from;
            mlp->max     = to;
            mlp->strand  = strand;
            mlp->noleft  = noleft;
            mlp->noright = noright;
            mlp->numint  = i++;
            continue;
        }

        for (tmlp = mlp;; tmlp = tmlp->next) {
            tempcirc = false;
            if (s_IdsMatch(pId, tmlp->pId) && tmlp->strand == strand) {
                if (tempcirc == false && ((tmlp->min <= to && tmlp->max >= from) ||
                                          fta_check_feat_overlap(gelocs, g, tmlp, from, to) == false)) {
                    if (tmlp->min > from) {
                        tmlp->min    = from;
                        tmlp->noleft = noleft;
                    }
                    if (tmlp->max < to) {
                        tmlp->max     = to;
                        tmlp->noright = noright;
                    }
                    break;
                }
            }

            if (tmlp->next)
                continue;

            tmlp->next    = new MixLoc();
            tmlp          = tmlp->next;
            tmlp->pId     = pId;
            tmlp->min     = from;
            tmlp->max     = to;
            tmlp->strand  = strand;
            tmlp->noleft  = noleft;
            tmlp->noright = noright;
            tmlp->numint  = i++;
            break;
        }
    }

    g.mlp = mlp;
}


/**********************************************************/
static void fta_make_seq_int(MixLocPtr mlp, bool noleft, bool noright, CSeq_interval& interval)
{
    if (mlp->strand != eNa_strand_unknown)
        interval.SetStrand(mlp->strand);

    interval.SetFrom(mlp->min);
    interval.SetTo(mlp->max);

    interval.SetId(*(mlp->pId));

    if (mlp->noleft || noleft) {
        interval.SetFuzz_from().SetLim(CInt_fuzz::eLim_lt);
    }

    if (mlp->noright || noright) {
        interval.SetFuzz_to().SetLim(CInt_fuzz::eLim_gt);
    }
}

/**********************************************************/
static void fta_make_seq_pnt(MixLocPtr mlp, bool noleft, bool noright, CSeq_point& point)
{
    if (mlp->strand != eNa_strand_unknown)
        point.SetStrand(mlp->strand);
    point.SetPoint(mlp->min);

    point.SetId(*(mlp->pId));

    if (mlp->noleft || mlp->noright || noleft || noright) {
        CInt_fuzz::TLim lim = (mlp->noleft == false && noleft == false) ? CInt_fuzz::eLim_gt : CInt_fuzz::eLim_lt;
        point.SetFuzz().SetLim(lim);
    }
}

/**********************************************************/
static CRef<CSeq_loc> MakeCLoc(MixLocPtr mlp, bool noleft, bool noright)
{
    CRef<CSeq_loc> ret(new CSeq_loc);

    if (! mlp->next) {
        if (mlp->min == mlp->max)
            fta_make_seq_pnt(mlp, noleft, noright, ret->SetPnt());
        else
            fta_make_seq_int(mlp, noleft, noright, ret->SetInt());
        return ret;
    }

    CRef<CSeq_loc> cur;
    CSeq_loc_mix&  mix = ret->SetMix();

    for (; mlp; mlp = mlp->next) {
        cur.Reset(new CSeq_loc);

        if (mlp->min == mlp->max) {
            fta_make_seq_pnt(mlp, noleft, noright, cur->SetPnt());
        } else {
            fta_make_seq_int(mlp, false, false, cur->SetInt());
        }

        mix.AddSeqLoc(*cur);
    }

    return ret;
}

/**********************************************************/
static Int2 GetMergeOrder(MixLocPtr first, MixLocPtr second)
{
    MixLocPtr mlp;
    MixLocPtr tmlp;
    Int2      count;

    count = 0;
    for (mlp = second; mlp; mlp = mlp->next) {
        if (! mlp->pId)
            continue;
        for (tmlp = second; tmlp < mlp; tmlp = tmlp->next)
            if (tmlp->pId && s_IdsMatch(tmlp->pId, mlp->pId))
                break;
        if (tmlp < mlp)
            continue;
        count++;
    }
    for (mlp = first; mlp; mlp = mlp->next) {
        if (! mlp->pId)
            continue;
        for (tmlp = first; tmlp < mlp; tmlp = tmlp->next)
            if (tmlp->pId && s_IdsMatch(tmlp->pId, mlp->pId))
                break;
        if (tmlp < mlp)
            continue;
        count--;
    }
    return (count);
}

/**********************************************************/
static void CircularSeqLocFormat(Gene& g)
{
    MixLocPtr mlp;
    MixLocPtr tmlp;
    MixLocPtr mlpprev;
    MixLocPtr mlpnext;

    if (! g.mlp || ! g.mlp->next)
        return;

    for (bool got = true; got == true;) {
        got = false;
        for (mlp = g.mlp; mlp; mlp = mlp->next) {
            if (mlp->numint == -1)
                continue;
            for (tmlp = mlp->next; tmlp; tmlp = tmlp->next) {
                if (tmlp->numint == -1 || mlp->strand != tmlp->strand)
                    continue;

                if (mlp->numint == 0 && tmlp->numint == 0) {
                    if ((tmlp->min >= mlp->min && tmlp->min <= mlp->max) ||
                        (tmlp->max >= mlp->min && tmlp->max <= mlp->max) ||
                        (mlp->min > tmlp->min && mlp->max < tmlp->max)) {
                        if (mlp->min == tmlp->min) {
                            if (tmlp->noleft)
                                mlp->noleft = true;
                        } else if (mlp->min > tmlp->min) {
                            mlp->min    = tmlp->min;
                            mlp->noleft = tmlp->noleft;
                        }
                        if (mlp->max == tmlp->max) {
                            if (tmlp->noright)
                                mlp->noright = true;
                        } else if (mlp->max < tmlp->max) {
                            mlp->max     = tmlp->max;
                            mlp->noright = tmlp->noright;
                        }
                        tmlp->numint = -1;
                        got          = true;
                    }
                    continue;
                }

                if (mlp->min != tmlp->min || mlp->max != tmlp->max)
                    continue;
                if (tmlp->numint == 0) {
                    if (tmlp->noleft)
                        mlp->noleft = true;
                    if (tmlp->noright)
                        mlp->noright = true;
                    tmlp->numint = -1;
                } else if (mlp->numint == 0) {
                    if (mlp->noleft)
                        tmlp->noleft = true;
                    if (mlp->noright)
                        tmlp->noright = true;
                    mlp->numint = -1;
                } else if (tmlp->numint >= mlp->numint) {
                    if (tmlp->noleft)
                        mlp->noleft = true;
                    if (tmlp->noright)
                        mlp->noright = true;
                    tmlp->numint = -1;
                } else {
                    if (mlp->noleft)
                        tmlp->noleft = true;
                    if (mlp->noright)
                        tmlp->noright = true;
                    mlp->numint = -1;
                }
                got = true;
            }
        }
    }

    for (mlpprev = nullptr, mlp = g.mlp; mlp; mlp = mlpnext) {
        mlpnext = mlp->next;
        if (mlp->numint != -1) {
            mlpprev = mlp;
            continue;
        }

        if (! mlpprev)
            g.mlp = mlpnext;
        else
            mlpprev->next = mlpnext;
        mlp->next = nullptr;
        MixLocFree(mlp);
    }

    mlpprev = nullptr;
    for (mlp = g.mlp; mlp; mlpprev = mlp, mlp = mlp->next)
        if (mlp->numint == 1)
            break;

    if (mlp && mlp != g.mlp) {
        mlpprev->next = nullptr;
        for (tmlp = mlp; tmlp->next;)
            tmlp = tmlp->next;
        tmlp->next = g.mlp;
        g.mlp      = mlp;
    }
}

/**********************************************************/
static void SortMixLoc(MixLocPtr& mll)
{
    MixLocPtr mlp;
    MixLocPtr tmlp;

    bool noleft;
    bool noright;

    Int4 min;
    Int4 max;
    Int4 numint;

    if (! mll || ! mll->next)
        return;

    for (mlp = mll; mlp; mlp = mlp->next) {
        for (tmlp = mlp->next; tmlp; tmlp = tmlp->next) {
            if (! s_IdsMatch(mlp->pId, tmlp->pId) ||
                mlp->strand != tmlp->strand)
                break;
            if (mlp->strand == eNa_strand_minus) {
                if (tmlp->min < mlp->min)
                    continue;
                if (tmlp->min == mlp->min) {
                    if (tmlp->noleft == mlp->noleft) {
                        if (tmlp->max < mlp->max)
                            continue;
                        if (tmlp->max == mlp->max) {
                            if (tmlp->noright == mlp->noright || mlp->noright)
                                continue;
                        }
                    } else if (mlp->noleft)
                        continue;
                }
            } else {
                if (tmlp->min > mlp->min)
                    continue;
                if (tmlp->min == mlp->min) {
                    if (tmlp->noleft == mlp->noleft) {
                        if (tmlp->max > mlp->max)
                            continue;
                        if (tmlp->max == mlp->max) {
                            if (tmlp->noright == mlp->noright || tmlp->noright)
                                continue;
                        }
                    } else if (tmlp->noleft)
                        continue;
                }
            }
            min           = mlp->min;
            max           = mlp->max;
            noleft        = mlp->noleft;
            noright       = mlp->noright;
            numint        = mlp->numint;
            mlp->min      = tmlp->min;
            mlp->max      = tmlp->max;
            mlp->noleft   = tmlp->noleft;
            mlp->noright  = tmlp->noright;
            mlp->numint   = tmlp->numint;
            tmlp->min     = min;
            tmlp->max     = max;
            tmlp->noleft  = noleft;
            tmlp->noright = noright;
            tmlp->numint  = numint;
        }
    }
}

/**********************************************************/
static void ScannGeneName(GeneNodePtr gnp, Int4 seqlen)
{
    GeneListPtr cn;

    MixLocPtr mlp;
    Int4      j;
    Int2      level;

    bool join;

    for (auto c = gnp->gl; c->next; c = c->next) {
        for (cn = c->next; cn; cn = cn->next) {
            if (c->feat.NotEmpty() && cn->feat.NotEmpty() &&
                c->feat->IsSetData() && c->feat->GetData().IsCdregion() &&
                cn->feat->IsSetData() && c->feat->GetData().IsCdregion() &&
                fta_cmp_locusyn(*c, *cn) == 0) {
                FtaErrPost(SEV_WARNING, ERR_GENEREF_NoUniqMaploc, "Two different cdregions for one gene {}\"{}\".", (c->locus.empty()) ? "with locus_tag " : "", (c->locus.empty()) ? c->locus_tag : c->locus);
            }
        }
    }

    bool circular = false;
    j             = 1;
    for (auto c = gnp->gl; c; c = c->next, j++) {
        FixMixLoc(*c, gnp->gelocs);
        if (gnp->circular && ConfirmCircular(c->mlp))
            circular = true;
    }

    gnp->circular = circular;


    for (auto c = gnp->gl; c->next;) {
        cn = c->next;
        if (fta_cmp_locusyn(*c, *cn) != 0 ||
            c->leave != 0 || cn->leave != 0 ||
            c->slibp->strand != cn->slibp->strand ||
            (gnp->simple_genes == false && DoWeHaveGeneInBetween(*c, cn->slibp, gnp))) {
            c = cn;
            continue;
        }

        if (gnp->skipdiv && ! gnp->cdsl.empty() && ! gnp->simple_genes &&
            DoWeHaveCdssInBetween(*c, cn->slibp->from, gnp->cdsl)) {
            c = cn;
            continue;
        }

        if (c->slibp->from > cn->slibp->from) {
            c->slibp->from   = cn->slibp->from;
            c->slibp->noleft = cn->slibp->noleft;
        }
        if (c->slibp->to < cn->slibp->to) {
            c->slibp->to      = cn->slibp->to;
            c->slibp->noright = cn->slibp->noright;
        }

        if (! gnp->simple_genes) {
            auto cp = gnp->gl;
            for (; cp; cp = cp->next) {
                if (cp->leave == 1 || cp->circular)
                    continue;
                if (fta_cmp_locusyn(*cp, *c) == 0 ||
                    cp->slibp->strand != c->slibp->strand)
                    continue;
                if (c->slibp->from <= cp->slibp->to &&
                    c->slibp->to >= cp->slibp->from)
                    break;
            }
            join = (cp != nullptr);
        } else
            join = false;

        if (! gnp->circular) {
            level = GetMergeOrder(c->mlp, cn->mlp);
            if (level > 0) {
                mlp     = EasySeqLocMerge(cn->mlp, c->mlp, join);
                c->feat = cn->feat;
            } else
                mlp = EasySeqLocMerge(c->mlp, cn->mlp, join);
        } else
            mlp = CircularSeqLocCollect(c->mlp, cn->mlp);

        if (c->mlp)
            MixLocFree(c->mlp);
        c->mlp = mlp;
        if (cn->pseudo)
            c->pseudo = true;
        if (! cn->allpseudo)
            c->allpseudo = false;
        fta_check_pseudogene(*cn, *c);
        fta_add_wormbase(*cn, *c);
        fta_add_olt(*cn, *c);
        c->noleft  = c->slibp->noleft;
        c->noright = c->slibp->noright;
        c->next    = cn->next;
        delete cn;
    }

    for (auto c = gnp->gl; c; c = c->next) {
        SortMixLoc(c->mlp);
        if (gnp->circular)
            CircularSeqLocFormat(*c);
    }


    for (auto c = gnp->gl; c; c = c->next)
        if (c->loc.Empty() && c->mlp)
            c->loc = MakeCLoc(c->mlp, c->noleft, c->noright);

    for (auto c = gnp->gl; c; c = c->next) {
        if (c->loc.Empty())
            continue;

        const CSeq_loc& loc = *c->loc;
        for (cn = gnp->gl; cn; cn = cn->next) {
            if (cn->loc.Empty() || &loc == cn->loc ||
                cn->slibp->strand != c->slibp->strand ||
                fta_cmp_locusyn(*cn, *c) != 0)
                continue;

            sequence::ECompare cmp_res = sequence::Compare(loc, *cn->loc, nullptr, sequence::fCompareOverlapping);
            if (cmp_res != sequence::eContains && cmp_res != sequence::eSame)
                continue;

            if (cn->leave == 1 || c->leave == 0)
                c->leave = cn->leave;
            if (cn->pseudo)
                c->pseudo = true;
            if (! cn->allpseudo)
                c->allpseudo = false;
            fta_check_pseudogene(*cn, *c);
            fta_add_wormbase(*cn, *c);
            fta_add_olt(*cn, *c);
            if (cn->noleft)
                c->noleft = true;
            if (cn->noright)
                c->noright = true;

            cn->loc.Reset();
        }
    }

    GeneListPtr cp = nullptr;
    for (auto c = gnp->gl; c; c = cn) {
        cn = c->next;
        if (! c->loc) {
            if (! cp)
                gnp->gl = cn;
            else
                cp->next = cn;
            delete c;
        } else
            cp = c;
    }

    for (auto c = gnp->gl; c; c = cn) {
        cn = c->next;
        if (! cn || fta_cmp_locusyn(*c, *cn) != 0) {
            AddGeneFeat(*c, c->maploc, gnp->feats);
            continue;
        }

        string maploc;
        for (cn = c; cn; cn = cn->next) {
            if (fta_cmp_locusyn(*c, *cn) != 0)
                break;
            if (cn->maploc.empty())
                continue;

            if (maploc.empty()) {
                maploc = cn->maploc;
            } else if (! NStr::EqualNocase(maploc, cn->maploc)) {
                FtaErrPost(SEV_WARNING, ERR_GENEREF_NoUniqMaploc, "Different maplocs in the gene {}\"{}\".", (c->locus.empty()) ? "with locus_tag " : "", (c->locus.empty()) ? c->locus_tag : c->locus);
            }
        }
        for (cn = c; cn; cn = cn->next) {
            if (fta_cmp_locusyn(*c, *cn) != 0)
                break;
            AddGeneFeat(*cn, maploc, gnp->feats);
        }
    }
}

/**********************************************************/
static CRef<CSeq_id> CpSeqIdAcOnly(const CSeq_id& id, bool accver)
{
    auto new_id = Ref(new CSeq_id());
    new_id->Assign(id);

    if (! accver) {
        const CTextseq_id* pTextId = new_id->GetTextseq_Id();
        if (pTextId && pTextId->IsSetVersion()) {
            const_cast<CTextseq_id*>(pTextId)->ResetVersion();
        }
    }

    return new_id;
}

/**********************************************************/
static bool WeDontNeedToJoinThis(const CSeqFeatData& data)
{
    const Char** b;
    short*       i;

    if (data.IsRna()) {
        const CRNA_ref& rna_ref = data.GetRna();

        i = nullptr;
        if (rna_ref.IsSetType()) {
            for (i = leave_rna_feat; *i != -1; i++) {
                if (rna_ref.GetType() == *i)
                    break;
            }
        }

        if (i && *i != -1)
            return true;

        if (rna_ref.GetType() == CRNA_ref::eType_other && rna_ref.IsSetExt() && rna_ref.GetExt().IsName() &&
            rna_ref.GetExt().GetName() == "ncRNA")
            return true;
    } else if (data.IsImp()) {
        b = nullptr;
        if (data.GetImp().IsSetKey()) {
            for (b = leave_imp_feat; *b; b++) {
                if (data.GetImp().GetKey() == *b)
                    break;
            }
        }
        if (b && *b)
            return true;
    }
    return false;
}

/**********************************************************/
static void GetGeneSyns(const TQualVector& quals, const string& name, TSynSet& syns)
{
    if (name.empty())
        return;

    for (const auto& qual : quals) {
        if (! qual->IsSetQual() || ! qual->IsSetVal() ||
            qual->GetQual() != "gene" ||
            NStr::EqualNocase(qual->GetVal(), name))
            continue;

        syns.insert(qual->GetVal());
    }

    for (const auto& qual : quals) {
        if (! qual->IsSetQual() || ! qual->IsSetVal() ||
            qual->GetQual() != "gene_synonym" ||
            NStr::EqualNocase(qual->GetVal(), name))
            continue;

        syns.insert(qual->GetVal());
    }
}

/**********************************************************/
static bool fta_rnas_cds_feat(const CSeq_feat& feat)
{
    if (! feat.IsSetData())
        return false;

    if (feat.GetData().IsImp()) {
        if (feat.GetData().GetImp().IsSetKey()) {
            const string& key = feat.GetData().GetImp().GetKey();
            if (key == "CDS" || key == "rRNA" ||
                key == "tRNA" || key == "mRNA")
                return true;
        }
        return false;
    }

    if (feat.GetData().IsCdregion())
        return true;

    if (! feat.GetData().IsRna())
        return false;

    const CRNA_ref& rna_ref = feat.GetData().GetRna();
    if (rna_ref.IsSetType() && rna_ref.GetType() > 1 && rna_ref.GetType() < 5) /* mRNA, tRNA or rRNA */
        return true;

    return false;
}

/**********************************************************/
static bool IfCDSGeneFeat(const CSeq_feat& feat, Uint1 choice, const char* key)
{
    if (feat.IsSetData() && feat.GetData().Which() == choice)
        return true;

    if (feat.GetData().IsImp()) {
        if (feat.GetData().GetImp().IsSetKey() && feat.GetData().GetImp().GetKey() == key)
            return true;
    }

    return false;
}

/**********************************************************/
static bool GetFeatNameAndLoc(Gene* glp, const CSeq_feat& feat, GeneNodePtr gnp)
{
    const char* p;

    bool ret = false;

    p = nullptr;
    if (feat.IsSetData()) {
        if (feat.GetData().IsImp()) {
            p = feat.GetData().GetImp().IsSetKey() ? feat.GetData().GetImp().GetKey().c_str() : nullptr;
        } else if (feat.GetData().IsCdregion())
            p = "CDS";
        else if (feat.GetData().IsGene())
            p = "gene";
        else if (feat.GetData().IsBiosrc())
            p = "source";
        else if (feat.GetData().IsRna()) {
            const CRNA_ref& rna_ref = feat.GetData().GetRna();

            if (rna_ref.IsSetType()) {
                switch (rna_ref.GetType()) {
                case 1:
                    p = "precursor_RNA";
                    break;
                case 2:
                    p = "mRNA";
                    break;
                case 3:
                    p = "tRNA";
                    break;
                case 4:
                    p = "rRNA";
                    break;
                case 5:
                    p = "snRNA";
                    break;
                case 6:
                    p = "scRNA";
                    break;
                case 7:
                    p = "snoRNA";
                    break;
                case 255:
                    p = "misc_RNA";
                    break;
                default:
                    p = "an RNA";
                }
            } else
                p = "an RNA";
        }
    }

    if (! p)
        p = "a";

    ret = (MatchArrayString(feat_no_gene, p) < 0);

    if (! glp)
        return (ret);

    if (StringEqu(p, "misc_feature"))
        gnp->got_misc = true;

    glp->fname = p;

    if (! feat.IsSetLocation()) {
        glp->location = "Unknown";
        return (ret);
    }

    string loc_str;
    GetLocationStr(feat.GetLocation(), loc_str);
    if (loc_str.empty())
        glp->location = "Unknown";
    else {
        if (loc_str.size() > 55) {
            loc_str = loc_str.substr(0, 50);
            loc_str += " ...";
        }
        glp->location = loc_str;
    }

    return (ret);
}

/**********************************************************/
static list<AccMinMax> fta_get_acc_minmax_strand(const CSeq_loc* location,
                                                 GeneLoc*        gelop)
{
    list<AccMinMax> ammps;
    Int4            from;
    Int4            to;

    gelop->strand = -2;

    if (location) {
        for (CSeq_loc_CI loc(*location); loc; ++loc) {
            CConstRef<CSeq_loc> cur_loc = loc.GetRangeAsSeq_loc();
            CRef<CSeq_id>       pId;
            if (cur_loc->IsInt()) {
                const CSeq_interval& interval = cur_loc->GetInt();
                if (interval.IsSetId()) {
                    pId = Ref(new CSeq_id());
                    pId->Assign(interval.GetId());
                }
                from = interval.GetFrom();
                to   = interval.GetTo();

                ENa_strand strand = interval.IsSetStrand() ? interval.GetStrand() : eNa_strand_unknown;
                if (gelop->strand == -2)
                    gelop->strand = strand;
                else if (gelop->strand != strand)
                    gelop->strand = -1;
            } else if (cur_loc->IsPnt()) {
                const CSeq_point& point = cur_loc->GetPnt();
                if (point.IsSetId()) {
                    pId = Ref(new CSeq_id());
                    pId->Assign(point.GetId());
                }
                from = point.GetPoint();
                to   = from;

                ENa_strand strand = point.IsSetStrand() ? point.GetStrand() : eNa_strand_unknown;
                if (gelop->strand == -2)
                    gelop->strand = strand;
                else if (gelop->strand != strand)
                    gelop->strand = -1;
            } else {
                continue;
            }

            _ASSERT(pId);

            if (gelop->verymin > from)
                gelop->verymin = from;
            if (gelop->verymax < to)
                gelop->verymax = to;

            bool found_id = false;
            auto it       = ammps.begin();
            while (! found_id && it != ammps.end()) {
                auto& ammp = *it;
                if (s_IdsMatch(ammp.pId, pId)) {
                    if (from < ammp.min) {
                        ammp.min = from;
                    }
                    if (to > ammp.max) {
                        ammp.max = to;
                    }
                    found_id = true;
                }
                ++it;
            }

            if (! found_id) {
                AccMinMax ammp;
                ammp.pId = pId;
                ammp.min = from;
                ammp.max = to;
                ammps.push_back(ammp);
            }
        }
    }
    return ammps;
}

/**********************************************************/
static void fta_append_feat_list(GeneNodePtr gnp, const CSeq_loc* location, const string& gene, const string& locus_tag)
{
    if (! gnp || ! location)
        return;

    auto& geloc = gnp->gelocs.emplace_front();
    geloc.gene    = gene;
    geloc.locus   = locus_tag;
    geloc.verymin = -1;
    geloc.verymax = -1;
    geloc.ammp    = fta_get_acc_minmax_strand(location, &geloc);
}

/**********************************************************/
static bool CompareGeneLocsMinMax(const GeneLoc& sp1, const GeneLoc& sp2)
{
    Int4 status = sp2.verymax - sp1.verymax;
    if (status == 0)
        status = sp1.verymin - sp2.verymin;

    return status < 0;
}

/**********************************************************/
static void fta_collect_wormbases(Gene& g, CSeq_feat& feat)
{
    if (! feat.IsSetDbxref())
        return;

    CSeq_feat::TDbxref dbxrefs;
    for (const auto& dbxref : feat.SetDbxref()) {
        if (! dbxref->IsSetTag() || ! dbxref->IsSetDb() ||
            dbxref->GetDb() != "WormBase"s ||
            ! dbxref->GetTag().GetStr().starts_with("WBGene"sv)) {
            dbxrefs.push_back(dbxref);
            continue;
        }

        g.wormbase.insert(dbxref->GetTag().GetStr());
    }

    if (dbxrefs.empty())
        feat.ResetDbxref();
    else
        feat.SetDbxref().swap(dbxrefs);
}

/**********************************************************/
static void fta_collect_olts(Gene& g, CSeq_feat& feat)
{
    if (! feat.IsSetQual())
        return;

    TQualVector quals;
    for (TQualVector::iterator qual = feat.SetQual().begin(); qual != feat.SetQual().end(); ++qual) {
        if (! (*qual)->IsSetQual() || ! (*qual)->IsSetVal() ||
            (*qual)->GetQual() != "old_locus_tag") {
            quals.push_back(*qual);
            continue;
        }

        g.olt.insert((*qual)->GetVal());
    }

    if (quals.empty())
        feat.ResetQual();
    else
        feat.SetQual().swap(quals);
}

/**********************************************************
 *
 *   SrchGene:
 *   -- add new gene qual information into "glp"
 *
 **********************************************************/
static void SrchGene(CSeq_annot::C_Data::TFtable& feats, GeneNodePtr gnp, Int4 length, const CSeq_id& id)
{
    if (! gnp)
        return;

    for (auto& feat : feats) {
        const string gene      = CpTheQualValue(feat->GetQual(), "gene");
        const string locus_tag = CpTheQualValue(feat->GetQual(), "locus_tag");

        const CSeq_loc* cur_loc = feat->IsSetLocation() ? &feat->GetLocation() : nullptr;
        if (gene.empty() && locus_tag.empty()) {
            if (GetFeatNameAndLoc(nullptr, *feat, gnp))
                fta_append_feat_list(gnp, cur_loc, gene, locus_tag);
            continue;
        }

        const string pseudogene = CpTheQualValue(feat->GetQual(), "pseudogene");

        Gene* newglp       = new Gene;
        newglp->locus      = gene;
        newglp->locus_tag  = locus_tag;
        newglp->pseudogene = pseudogene;

        fta_collect_wormbases(*newglp, *feat);
        fta_collect_olts(*newglp, *feat);
        if (GetFeatNameAndLoc(newglp, *feat, gnp))
            fta_append_feat_list(gnp, cur_loc, gene, locus_tag);

        newglp->feat.Reset();
        if (gnp->simple_genes == false && cur_loc && cur_loc->IsMix()) {
            newglp->feat.Reset(new CSeq_feat);
            newglp->feat->Assign(*feat);
        }

        newglp->slibp = GetLowHighFromSeqLoc(cur_loc, length, id);
        if (! newglp->slibp) {
            delete newglp;
            continue;
        }
        if (gnp->simple_genes == false && feat->IsSetData() &&
            WeDontNeedToJoinThis(feat->GetData()))
            newglp->leave = 1;

        newglp->genefeat = IfCDSGeneFeat(*feat, CSeqFeatData::e_Gene, "gene");

        if (feat->IsSetQual()) {
            auto qual = GetTheQualValue(feat->SetQual(), "map");
            if (qual) {
                newglp->maploc = *qual;
            }
        }

        GetGeneSyns(feat->GetQual(), gene, newglp->syn);

        newglp->loc.Reset();
        if (cur_loc) {
            newglp->loc.Reset(new CSeq_loc);
            newglp->loc->Assign(*cur_loc);
        }

        newglp->todel = false;
        if (IfCDSGeneFeat(*feat, CSeqFeatData::e_Cdregion, "CDS") == false && newglp->genefeat == false)
            newglp->pseudo = false;
        else
            newglp->pseudo = feat->IsSetPseudo() ? feat->GetPseudo() : false;

        newglp->allpseudo = feat->IsSetPseudo() ? feat->GetPseudo() : false;

        if (fta_rnas_cds_feat(*feat)) {
            newglp->noleft  = newglp->slibp->noleft;
            newglp->noright = newglp->slibp->noright;
        } else {
            newglp->noleft  = false;
            newglp->noright = false;
        }

        newglp->next = gnp->gl;
        gnp->gl      = newglp;
    }

    if (! gnp->gelocs.empty())
        gnp->gelocs.sort(CompareGeneLocsMinMax);
}

/**********************************************************/
static void SrchCDSs(CSeq_annot::C_Data::TFtable& feats, TCdsList& cdsl, const CSeq_id& id)
{
    for (const auto& feat : feats) {
        if (IfCDSGeneFeat(*feat, CSeqFeatData::e_Cdregion, "CDS") == false)
            continue;

        const CSeq_loc* cur_loc = feat->IsSetLocation() ? &feat->GetLocation() : nullptr;

        const SeqlocInfoblk* slip = GetLowHighFromSeqLoc(cur_loc, -99, id);
        if (! slip)
            continue;

        auto& newclp = cdsl.emplace_front();
        newclp.from  = slip->from;
        newclp.to    = slip->to;
        delete slip;
    }
}


/**********************************************************
 *
 *   FindGene:
 *      -- there is no accession number if it is a segmented
 *         set entry.
 *
 **********************************************************/
static void FindGene(CBioseq& bioseq, GeneNodePtr gene_node)
{
    const CSeq_id* first_id = nullptr;
    if (! bioseq.GetId().empty())
        first_id = *bioseq.GetId().begin();

    if (! first_id) {
        return;
    }

    if (bioseq.GetInst().GetTopology() == CSeq_inst::eTopology_circular)
        gene_node->circular = true;

    if (! bioseq.IsSetAnnot())
        return;

    for (auto& annot : bioseq.SetAnnot()) {
        if (! annot->IsFtable())
            continue;

        CRef<CSeq_id> id = CpSeqIdAcOnly(*first_id, gene_node->accver);

        SrchGene(annot->SetData().SetFtable(), gene_node, bioseq.GetLength(), *id);

        if (gene_node->skipdiv) {
            SrchCDSs(annot->SetData().SetFtable(), gene_node->cdsl, *id);
        }

        if (gene_node->gl && gene_node->flag == false) {
            /* the seqentry is not a member of segment seqentry
             */
            gene_node->bioseq = &bioseq;
            gene_node->flag   = true;
        }

        break;
    }
}

/**********************************************************/
static void GeneCheckForStrands(const TGeneList& gl)
{
    if (! gl)
        return;

    auto glp = gl;
    while (glp) {
        if (glp->locus.empty() && glp->locus_tag.empty())
            continue; // infinite loop ?
        bool got  = false;
        auto tglp = glp->next;
        for (; tglp; tglp = tglp->next) {
            if (tglp->locus.empty() && tglp->locus_tag.empty())
                continue;
            if (fta_cmp_locusyn(*glp, *tglp) != 0)
                break;
            if (! got && glp->slibp && tglp->slibp &&
                glp->slibp->strand != tglp->slibp->strand)
                got = true;
        }
        if (got) {
            FtaErrPost(SEV_WARNING, ERR_GENEREF_BothStrands, "Gene name {}\"{}\" has been used for features on both strands.", (glp->locus.empty()) ? "with locus_tag " : "", (glp->locus.empty()) ? glp->locus_tag : glp->locus);
        }
        glp = tglp;
    }
}

/**********************************************************/
static bool LocusTagCheck(TGeneList& gl, bool& resort)
{
    GeneListPtr glpstart;
    GeneListPtr glpstop;

    bool same_gn;
    bool same_lt;
    bool ret;

    resort = false;
    if (! gl || ! gl->next)
        return true;

    glpstop = nullptr;
    ret     = true;
    for (auto glp = gl; glp; glp = glpstop->next) {
        if (glp->locus.empty() && glp->locus_tag.empty())
            continue;

        glpstart = glp;
        glpstop  = glp;
        for (auto tglp = glp->next; tglp; tglp = tglp->next) {
            if (NStr::EqualNocase(glp->locus, tglp->locus) == false ||
                NStr::EqualNocase(glp->locus_tag, tglp->locus_tag) == false)
                break;
            glpstop = tglp;
        }

        for (auto tglp = glpstop->next; tglp; tglp = tglp->next) {
            if (tglp->locus.empty() && tglp->locus_tag.empty())
                continue;

            same_gn = NStr::EqualNocase(glpstart->locus, tglp->locus);
            same_lt = NStr::EqualNocase(glpstart->locus_tag, tglp->locus_tag);

            if ((same_gn == false && same_lt == false) || (same_gn && same_lt) ||
                same_gn || glpstart->locus_tag.empty())
                continue;

            for (glp = glpstart;; glp = glp->next) {
                FtaErrPost(SEV_REJECT, ERR_FEATURE_InconsistentLocusTagAndGene, "Inconsistent pairs /gene+/locus_tag are encountered: \"{}\"+\"{}\" : {} feature at {} : \"{}\"+\"{}\" : {} feature at {}. Entry dropped.", (glp->locus.empty()) ? "(NULL)" : glp->locus, (glp->locus_tag.empty()) ? "(NULL)" : glp->locus_tag, glp->fname, glp->location, (tglp->locus.empty()) ? "(NULL)" : tglp->locus, (tglp->locus_tag.empty()) ? "(NULL)" : tglp->locus_tag, tglp->fname, tglp->location);
                if (glp == glpstop)
                    break;
            }
            ret = false;
        }

        if (! glpstart->locus.empty() && ! glpstart->locus_tag.empty() &&
            glpstart->locus == glpstart->locus_tag) {
            for (glp = glpstart;; glp = glp->next) {
                glp->locus.clear();
                resort = true;
                if (glp == glpstop)
                    break;
            }
        }
    }

    return (ret);
}

/**********************************************************/
static void MiscFeatsWithoutGene(GeneNodePtr gnp)
{
    if (! gnp || ! gnp->gl)
        return;

    for (auto glp = gnp->gl; glp; glp = glp->next) {
        if (glp->locus_tag.empty() || ! glp->locus.empty() ||
            (glp->fname != "misc_feature"))
            continue;

        for (auto tglp = gnp->gl; tglp; tglp = tglp->next) {
            if (tglp->fname.empty() ||
                (tglp->fname == "misc_feature")) { // Looks suspicious - check again
                continue;
            }
            if (tglp->locus.empty() || tglp->locus[0] == '\0' ||
                ! NStr::EqualNocase(glp->locus_tag, tglp->locus_tag))
                continue;
            glp->locus = tglp->locus;
            break;
        }
    }
}

/**********************************************************/
static void RemoveUnneededMiscFeats(GeneNodePtr gnp)
{
    if (! gnp || ! gnp->gl)
        return;

    for (auto glp = gnp->gl; glp; glp = glp->next) {
        if (glp->todel || ! glp->syn.empty() || (glp->fname != "misc_feature"))
            continue;

        bool got = false;
        for (auto tglp = gnp->gl; tglp; tglp = tglp->next) {
            if (tglp->todel || (tglp->fname == "misc_feature"))
                continue;
            if (! NStr::EqualNocase(glp->locus, tglp->locus) ||
                ! NStr::EqualNocase(glp->locus_tag, tglp->locus_tag))
                continue;
            if (tglp->syn.empty()) {
                got = true;
                continue;
            }

            sequence::ECompare cmp_res = sequence::Compare(*glp->loc, *tglp->loc, nullptr, sequence::fCompareOverlapping);
            if (cmp_res != sequence::eContained)
                continue;

            glp->todel = true;
        }
        if (glp->todel && got)
            glp->todel = false;
    }

    GeneListPtr glpprev = nullptr;
    for (auto glp = gnp->gl; glp;) {
        auto glpnext = glp->next;
        if (! glp->todel) {
            glp->loc.Reset();
            glpprev = glp;
            glp     = glpnext;
            continue;
        }

        if (! glpprev)
            gnp->gl = glpnext;
        else
            glpprev->next = glpnext;

        delete glp;
        glp = glpnext;
    }
}

/**********************************************************/
static bool GeneLocusCheck(const TSeqFeatList& feats, bool diff_lt)
{
    bool ret = true;

    for (TSeqFeatList::const_iterator feat = feats.begin(); feat != feats.end(); ++feat) {
        const CGene_ref& gene_ref1 = (*feat)->GetData().GetGene();
        if (! gene_ref1.IsSetLocus() || ! gene_ref1.IsSetLocus_tag())
            continue;

        TSeqFeatList::const_iterator feat_next = feat,
                                     feat_cur  = feat;
        for (++feat_next; feat_next != feats.end(); ++feat_next, ++feat_cur) {
            const CGene_ref& gene_ref2 = (*feat_next)->GetData().GetGene();

            if (! gene_ref2.IsSetLocus() || ! gene_ref2.IsSetLocus_tag())
                continue;

            if (gene_ref1.GetLocus() != gene_ref2.GetLocus()) {
                feat = feat_cur;
                break;
            }

            if (gene_ref1.GetLocus_tag() == gene_ref2.GetLocus_tag())
                continue;

            string loc1_str, loc2_str;

            GetLocationStr((*feat)->GetLocation(), loc1_str);
            GetLocationStr((*feat_next)->GetLocation(), loc2_str);

            if (diff_lt == false) {
                FtaErrPost(SEV_REJECT,
                          ERR_FEATURE_MultipleGenesDifferentLocusTags,
                          "Multiple instances of the \"{}\" gene encountered: \"{}\"+\"{}\" : gene feature at \"{}\" : \"{}\"+\"{}\" : gene feature at \"{}\". Entry dropped.",
                          gene_ref1.GetLocus(),
                          gene_ref1.GetLocus(),
                          gene_ref1.GetLocus_tag(),
                          loc1_str,
                          gene_ref2.GetLocus(),
                          gene_ref2.GetLocus_tag(),
                          loc2_str);
                ret = false;
            } else
                FtaErrPost(SEV_WARNING,
                          ERR_FEATURE_MultipleGenesDifferentLocusTags,
                          "Multiple instances of the \"{}\" gene encountered: \"{}\"+\"{}\" : gene feature at \"{}\" : \"{}\"+\"{}\" : gene feature at \"{}\".",
                          gene_ref1.GetLocus(),
                          gene_ref1.GetLocus(),
                          gene_ref1.GetLocus_tag(),
                          loc1_str,
                          gene_ref2.GetLocus(),
                          gene_ref2.GetLocus_tag(),
                          loc2_str);
        }
    }

    return (ret);
}

/**********************************************************/
static void CheckGene(CRef<CSeq_entry> entry, ParserPtr pp, GeneRefFeats& gene_refs)
{
    IndexblkPtr ibp;
    GeneNodePtr gnp;

    char* div;

    bool resort;

    if (! pp)
        return;

    ibp = pp->entrylist[pp->curindx];
    if (! ibp)
        return;

    div = ibp->division;

    gnp               = new GeneNode;
    gnp->accver       = pp->accver;
    gnp->circular     = false;
    gnp->simple_genes = pp->simple_genes;
    gnp->got_misc     = false;
    if (div && (StringEqu(div, "BCT") || StringEqu(div, "SYN")))
        gnp->skipdiv = true;
    else
        gnp->skipdiv = false;

    if (entry) {
        for (CTypeIterator<CBioseq> bioseq(Begin(*entry)); bioseq; ++bioseq) {
            FindGene(*bioseq, gnp);
        }
    }   

    if (gnp->got_misc) {
        MiscFeatsWithoutGene(gnp);
        RemoveUnneededMiscFeats(gnp);
    } else {
        for (auto glp = gnp->gl; glp; glp = glp->next) {
            glp->loc.Reset();
        }
    }

    if (gnp->gl) {
        sort_gnp_list(gnp->gl);

        resort = false;
        if (LocusTagCheck(gnp->gl, resort) == false) {
            ibp->drop = true;
            GeneListFree(gnp->gl);
            delete gnp;
            return;
        }

        if (resort)
            sort_gnp_list(gnp->gl);

        ScannGeneName(gnp, gnp->bioseq ? gnp->bioseq->GetLength() : 0);

        if (GeneLocusCheck(gnp->feats, pp->diff_lt) == false) {
            ibp->drop = true;
            GeneListFree(gnp->gl);
            delete gnp;
            return;
        }

        if (gnp->circular == false || ibp->got_plastid == false)
            GeneCheckForStrands(gnp->gl);

        if (! gnp->feats.empty()) {
            auto& annots = gnp->bioseq->SetAnnot();

            for (auto& cur_annot : annots) {
                if (! cur_annot->IsFtable())
                    continue;

                size_t advance = cur_annot->GetData().GetFtable().size();
                cur_annot->SetData().SetFtable().splice(cur_annot->SetData().SetFtable().end(), gnp->feats);

                gene_refs.first = cur_annot->SetData().SetFtable().begin();
                std::advance(gene_refs.first, advance);
                gene_refs.last  = cur_annot->SetData().SetFtable().end();
                gene_refs.valid = true;
                break;
            }

            if (annots.empty()) {
                CRef<CSeq_annot> annot(new CSeq_annot);
                annot->SetData().SetFtable().assign(gnp->feats.begin(), gnp->feats.end());
                gnp->bioseq->SetAnnot().push_back(annot);

                gene_refs.first = annot->SetData().SetFtable().begin();
                gene_refs.last  = annot->SetData().SetFtable().end();
                gene_refs.valid = true;
            }
        }

        GeneListFree(gnp->gl);
    }

    delete gnp;
}

bool GenelocContained(
    const CSeq_loc& loc1,
    const CSeq_loc& loc2,
    CScope*         scope)
{
    const auto strand1 = loc1.GetStrand() == eNa_strand_minus ? eNa_strand_minus : eNa_strand_plus;
    const auto strand2 = loc2.GetStrand() == eNa_strand_minus ? eNa_strand_minus : eNa_strand_plus;
    if (strand1 != strand2) {
        return false;
    }
    if (loc1.IsInt() && loc2.IsInt()) {
        const auto& intv1 = loc1.GetInt();
        const auto& intv2 = loc2.GetInt();
        return (intv1.GetFrom() >= intv2.GetFrom() && intv1.GetTo() <= intv2.GetTo());
    }
    auto compResult = sequence::Compare(
        loc1, loc2, nullptr, sequence::fCompareOverlapping);
    return (compResult == sequence::eContained || compResult == sequence::eSame);
}


/**********************************************************
 *
 *   SeqFeatXrefPtr GetXrpForOverlap(glap, sfp, gerep):
 *
 *      Get xrp from list by locus only if cur gene overlaps
 *   other gene and asn2ff cannot find it.
 *
 **********************************************************/
static CRef<CSeqFeatXref> GetXrpForOverlap(
    const char*            acnum,
    GeneRefFeats&          gene_refs,
    const TSeqLocInfoList& llocs,
    const CSeq_feat&       feat,
    CGene_ref&             gerep)
{
    int count = 0;
    /*
    ENa_strand strand = feat.GetLocation().IsSetStrand() ? feat.GetLocation().GetStrand() : eNa_strand_unknown;
    if (strand == eNa_strand_other)
        strand = eNa_strand_unknown;
     */

    CConstRef<CGene_ref> gene_ref;

    TSeqLocInfoList::const_iterator cur_loc = llocs.begin();
    CRef<CSeq_loc>                  loc     = fta_seqloc_local(feat.GetLocation(), acnum); // passed as consts

    bool stopped = false;
    if (gene_refs.valid) {
        for (auto cur_feat = gene_refs.first; cur_feat != gene_refs.last; ++cur_feat) {
            if (! GenelocContained(*loc, *cur_loc->loc, nullptr)) {
                ++cur_loc;
                continue; /* f location is within sfp one */
            }

            count++;
            if (gene_ref.Empty()) {
                gene_ref.Reset(&(*cur_feat)->GetData().GetGene());
            } else if (fta_cmp_gene_refs(*gene_ref, (*cur_feat)->GetData().GetGene())) {
                stopped = true;
                break;
            }

            ++cur_loc;
        }
    }

    CRef<CSeqFeatXref> xref;

    if (count == 0 || (! stopped && gene_ref.NotEmpty() && fta_cmp_gene_refs(*gene_ref, gerep) == 0))
        return xref;

    xref.Reset(new CSeqFeatXref);
    xref->SetData().SetGene(gerep);

    return xref;
}

static void FixAnnot(CBioseq::TAnnot& annots, const char* acnum, GeneRefFeats& gene_refs, TSeqLocInfoList& llocs)
{
    for (CBioseq::TAnnot::iterator annot = annots.begin(); annot != annots.end();) {
        if (! (*annot)->IsSetData() || ! (*annot)->GetData().IsFtable()) {
            ++annot;
            continue;
        }

        CSeq_annot::C_Data::TFtable& feat_table = (*annot)->SetData().SetFtable();
        for (TSeqFeatList::iterator feat = feat_table.begin(); feat != feat_table.end();) {
            if ((*feat)->IsSetData() && (*feat)->GetData().IsImp()) {
                const CImp_feat& imp = (*feat)->GetData().GetImp();
                if (imp.GetKey() == "gene") {
                    feat = feat_table.erase(feat);
                    continue;
                }
            }

            optional<string> gene, locus_tag;
            if ((*feat)->IsSetQual()) {
                gene      = GetTheQualValue((*feat)->SetQual(), "gene");
                locus_tag = GetTheQualValue((*feat)->SetQual(), "locus_tag");
            }
            if (! gene && ! locus_tag) {
                ++feat;
                continue;
            }

            CRef<CGene_ref> gene_ref(new CGene_ref);
            if (gene) {
                gene_ref->SetLocus(*gene);
                TSynSet syns;
                GetGeneSyns((*feat)->GetQual(), *gene, syns);
                if (! syns.empty())
                    gene_ref->SetSyn().assign(syns.begin(), syns.end());
            }
            if (locus_tag)
                gene_ref->SetLocus_tag(*locus_tag);

            CRef<CSeqFeatXref> xref = GetXrpForOverlap(acnum, gene_refs, llocs, *(*feat), *gene_ref);
            if (xref.NotEmpty())
                (*feat)->SetXref().push_back(xref);

            DeleteQual((*feat)->SetQual(), "gene");
            DeleteQual((*feat)->SetQual(), "locus_tag");
            DeleteQual((*feat)->SetQual(), "gene_synonym");

            if ((*feat)->GetQual().empty())
                (*feat)->ResetQual();
            ++feat;
        }

        if (feat_table.empty())
            annot = annots.erase(annot);
        else
            ++annot;
    }
}

/**********************************************************
 *
 *   GeneQuals:
 *   -- find match_gene Gene-ref for qual /gene
 *   -- find best_gene Gene-ref
 *               remove qual
 *               make Xref if best_gene and match_gene don't match
 *               remove misc_feat 'gene'
 *
 **********************************************************/
static void GeneQuals(CSeq_entry& entry, const char* acnum, GeneRefFeats& gene_refs)
{
    TSeqLocInfoList llocs;
    if (gene_refs.valid) {
        for (TSeqFeatList::iterator feat = gene_refs.first; feat != gene_refs.last; ++feat) {
            SeqLocInfo info;
            info.strand = (*feat)->GetLocation().IsSetStrand() ? (*feat)->GetLocation().GetStrand() : eNa_strand_unknown;

            if (info.strand == eNa_strand_other)
                info.strand = eNa_strand_unknown;

            info.loc = fta_seqloc_local((*feat)->GetLocation(), acnum);
            llocs.push_back(info);
        }
    }

    for (CTypeIterator<CBioseq_set> bio_set(Begin(entry)); bio_set; ++bio_set) {
        if (bio_set->IsSetAnnot())
            FixAnnot(bio_set->SetAnnot(), acnum, gene_refs, llocs);
    }

    for (CTypeIterator<CBioseq> bioseq(Begin(entry)); bioseq; ++bioseq) {
        if (bioseq->IsSetAnnot())
            FixAnnot(bioseq->SetAnnot(), acnum, gene_refs, llocs);
    }
}

/**********************************************************/
static void fta_collect_genes(const CBioseq& bioseq, std::set<string>& genes)
{
    for (const auto& annot : bioseq.GetAnnot()) {
        if (! annot->IsFtable())
            continue;

        for (const auto& feat : annot->GetData().GetFtable()) {
            for (const auto& qual : feat->GetQual()) {
                if (! qual->IsSetQual() || qual->GetQual() != "gene" ||
                    ! qual->IsSetVal() || qual->GetVal().empty())
                    continue;

                genes.insert(qual->GetVal());
            }
        }
    }
}

/**********************************************************/
static void fta_fix_labels(CBioseq& bioseq, const std::set<string>& genes)
{
    if (! bioseq.IsSetAnnot())
        return;

    for (auto& annot : bioseq.SetAnnot()) {
        if (! annot->IsFtable())
            continue;

        for (auto& feat : annot->SetData().SetFtable()) {

            if (! feat->IsSetQual())
                continue;

            for (CSeq_feat::TQual::iterator qual = feat->SetQual().begin(); qual != feat->SetQual().end(); ++qual) {
                if (! (*qual)->IsSetQual() || (*qual)->GetQual() != "label" ||
                    ! (*qual)->IsSetVal() || (*qual)->GetVal().empty())
                    continue;

                const string&                    cur_val = (*qual)->GetVal();
                std::set<string>::const_iterator ci      = genes.lower_bound(cur_val);
                if (*ci == cur_val) {
                    CRef<CGb_qual> new_qual(new CGb_qual);
                    new_qual->SetQual("gene");
                    new_qual->SetVal(cur_val);

                    feat->SetQual().insert(qual, new_qual);
                }
            }
        }
    }
}

/**********************************************************/
void DealWithGenes(CRef<CSeq_entry>& pEntry, ParserPtr pp)
{
    if (pp->source == Parser::ESource::Flybase) {
        std::set<string> genes;
        for (CBioseq_CI bioseq(GetScope(), *pEntry); bioseq; ++bioseq) {
            fta_collect_genes(*bioseq->GetCompleteBioseq(), genes);
        }

        if (! genes.empty()) {
            for (CTypeIterator<CBioseq> bioseq(Begin(*pEntry)); bioseq; ++bioseq) {
                fta_fix_labels(*bioseq, genes);
            }
        }
    }

    /* make GeneRefBlk if any gene qualifier exists
     */
    GeneRefFeats gene_refs;
    gene_refs.valid = false;

    CheckGene(pEntry, pp, gene_refs);

    if (gene_refs.valid) {
        for (TSeqFeatList::iterator feat = gene_refs.first; feat != gene_refs.last; ++feat) {
            if ((*feat)->IsSetLocation()) {
                int partial = sequence::SeqLocPartialCheck((*feat)->GetLocation(), &GetScope());
                if (partial & sequence::eSeqlocPartial_Start ||
                    partial & sequence::eSeqlocPartial_Stop) // not internal
                    (*feat)->SetPartial(true);

                if (! pp->genenull || ! (*feat)->GetLocation().IsMix())
                    continue;

                CSeq_loc_mix& mix_loc = (*feat)->SetLocation().SetMix();

                CRef<CSeq_loc> null_loc(new CSeq_loc);
                null_loc->SetNull();

                CSeq_loc_mix::Tdata::iterator it_loc = mix_loc.Set().begin();
                ++it_loc;
                for (; it_loc != mix_loc.Set().end(); ++it_loc) {
                    it_loc = mix_loc.Set().insert(it_loc, null_loc);
                    ++it_loc;
                }
            }
        }
    }

    ProcNucProt(pp, pEntry, gene_refs, GetScope());

    if (pEntry) {
        GeneQuals(*pEntry, pp->entrylist[pp->curindx]->acnum, gene_refs);
    }
}

END_NCBI_SCOPE
