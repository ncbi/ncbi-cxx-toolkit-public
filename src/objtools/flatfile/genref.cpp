/* genref.c
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
 * File Name:  genref.c
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 * -----------------
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
#    undef THIS_FILE
#endif
#define THIS_FILE "genref.cpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

struct SeqLocInfo
{
    CRef<CSeq_loc> loc;
    Uint1 strand;
};

typedef std::list<SeqLocInfo> TSeqLocInfoList;
typedef std::set<std::string> TSynSet;
typedef std::set<std::string> TWormbaseSet;
typedef std::set<std::string> TLocusTagSet;

struct AccMinMax {
    CRef<CSeq_id> pId;
//    string acc;
//    Int2        ver=INT2_MIN;
    Int4        min=-1;
    Int4        max=-1;
};

using AccMinMaxPtr = AccMinMax*;

struct GeneLocs {
    string               gene;
    string               locus;
    Int4                 strand;
    Int4                 verymin;
    Int4                 verymax;
    list<AccMinMax>      ammp;
    GeneLocs* next;
};

using GeneLocsPtr = GeneLocs*;


struct SeqlocInfoblk {
    Int4     from=0;                      /* the lowest value in the entry */
    Int4     to=0;                        /* the highest value in the entry */
    Uint1    strand=0;
    Int4     length=0;                    /* total length of the seq-data
                                           of the entry */
    TSeqIdList ids;                     /* the entry's SeqId */
    bool  noleft=0;
    bool  noright=0;
};

using SeqlocInfoblkPtr = SeqlocInfoblk*;

struct MixLoc {
    CRef<CSeq_id> pId;
    Int4    min;
    Int4    max;
    Uint1   strand;
    bool    noleft;
    bool    noright;
    Int4    numint;
    MixLoc* next=nullptr;
};

using MixLocPtr = MixLoc*;

struct GeneList {
    string               locus;        /* the name of the gene,
                                           copy the value */
    string               locus_tag;
    string               pseudogene;
    string               maploc;       /* the map of the gene,
                                           copy the value */

    CRef<CSeq_feat> feat;

    SeqlocInfoblkPtr      slibp=nullptr;        /* the location, points to the value */
    Int4                  segnum=0;       /* segment number */
    Uint1                 leave=0;        /* TRUE for aka tRNAs */

    CRef<CSeq_loc> loc;

    MixLocPtr             mlp=nullptr;

    TSynSet               syn;
    TWormbaseSet          wormbase;
    TLocusTagSet          olt;          /* /old_locus_tag values */

    bool                  pseudo=false;
    bool                  allpseudo=false;
    bool                  genefeat=false;
    bool                  noleft=false;
    bool                  noright=false;
    string                fname;
    string                location;
    bool                  todel = false;
    bool                  circular = false;
    GeneList* next=nullptr;
};

using GeneListPtr = GeneList*;

struct CdssList {
    Int4                  from;
    Int4                  to;
    Int4                  segnum;
    CdssList*             next;
};

using CdssListPtr = CdssList*;

struct GeneNode {
    bool        flag;                   /* TRUE, if a level has been found
                                           to put GeneRefPtr */
    bool        seg;                    /* TRUE, if this is a segment set
                                           entries */
    CBioseq* bioseq;
    CBioseq_set* bioseq_set;

    GeneListPtr glp;                    /* a list of gene infomation for
                                           the entries */
    TSeqFeatList feats;                  /* a list which contains the
                                           GeneRefPtr only */
    Int4        segindex;               /* total segments in this set */
    bool        accver;                 /* for ACCESSION.VERSION */
    bool        skipdiv;                /* skip BCT and SYN divisions */
    CdssListPtr clp;
    bool        circular;
    GeneLocsPtr gelop;
    bool        simple_genes;
    bool        got_misc;               /* TRUE if there is a misc_feature
                                           with gene or/and locus_tag */

    GeneNode() :
        flag(false),
        seg(false),
        bioseq(nullptr),
        bioseq_set(nullptr),
        glp(NULL),
        segindex(0),
        accver(false),
        skipdiv(false),
        clp(NULL),
        circular(false),
        gelop(NULL),
        simple_genes(false),
        got_misc(false)
    {}
};

using GeneNodePtr = GeneNode*;

/* The list of feature which are not allowed to have gene qual
 */
const char *feat_no_gene[] = {"gap", "operon", "source", NULL};

const char *leave_imp_feat[] = {
    "LTR",
    "conflict",
    "rep_origin",
    "repeat_region",
    "satellite",
    NULL
};

Int2 leave_rna_feat[] = {
      3,                                /* tRNA */
      4,                                /* rRNA */
      5,                                /* snRNA */
      6,                                /* scRNA */
     -1
};

/**********************************************************/
static void GetLocationStr(const CSeq_loc& loc, std::string& str)
{
    loc.GetLabel(&str);
    MakeLocStrCompatible(str);
}

/**********************************************************/
static bool fta_seqid_same(const CSeq_id& sid, const Char* acnum, const CSeq_id* id)
{
    if (id != NULL)
        return sid.Compare(*id) == CSeq_id::e_YES;

    if(acnum == NULL)
        return true;

    auto id_string = sid.GetSeqIdString();
    return NStr::EqualCase(id_string, acnum);
/*
    if (!sid.IsGenbank() && !sid.IsEmbl() && !sid.IsDdbj() &&
        !sid.IsPir() && !sid.IsSwissprot() && !sid.IsOther() &&
        !sid.IsPrf() && !sid.IsTpg() && !sid.IsTpd() &&
        !sid.IsTpe() && !sid.IsGpipe())
        return false;

    const CTextseq_id* text_id = sid.GetTextseq_Id();
    if (text_id == NULL || !text_id->IsSetAccession() ||
        text_id->GetAccession() != acnum)
        return false;

    return true;
    */
}

/**********************************************************/
static void fta_seqloc_del_far(CSeq_loc& locs, const Char* acnum, const CSeq_id* id)
{
    std::vector<CConstRef<CSeq_loc> > to_remove;

    for (CSeq_loc_CI ci(locs, CSeq_loc_CI::eEmpty_Allow); ci != locs.end(); ++ci)
    {
        CConstRef<CSeq_loc> cur_loc = ci.GetRangeAsSeq_loc();
        if (cur_loc->IsWhole())
        {
            if (fta_seqid_same(*cur_loc->GetId(), acnum, id))
                continue;
        }
        else if (cur_loc->IsInt())
        {
            if (fta_seqid_same(cur_loc->GetInt().GetId(), acnum, id))
                continue;
        }
        else if (cur_loc->IsPnt())
        {
            if (fta_seqid_same(cur_loc->GetPnt().GetId(), acnum, id))
                continue;
        }
        else if (cur_loc->IsBond())
        {
            if (cur_loc->GetBond().IsSetA() &&
                fta_seqid_same(cur_loc->GetBond().GetA().GetId(), acnum, id))
                continue;
        }
        else if (cur_loc->IsPacked_pnt())
        {
            if (fta_seqid_same(cur_loc->GetPacked_pnt().GetId(), acnum, id))
                continue;
        }

        to_remove.push_back(cur_loc);
    }

    for (std::vector<CConstRef<CSeq_loc> >::const_iterator it = to_remove.begin(); it != to_remove.end(); ++it)
        locs.Assign(*locs.Subtract(*(*it), 0, nullptr, nullptr));

    for (CTypeIterator<CSeq_bond> bond(locs); bond; ++bond)
    {
        if (bond->IsSetB() && !fta_seqid_same(bond->GetB().GetId(), acnum, id))
            bond->ResetB();
    }
}


/**********************************************************/
static CRef<CSeq_loc> fta_seqloc_local(const CSeq_loc& orig, const Char* acnum)
{
    CRef<CSeq_loc> ret(new CSeq_loc);
    ret->Assign(orig);

    if(acnum != NULL && *acnum != '\0' && *acnum != ' ')
        fta_seqloc_del_far(*ret, acnum, NULL);

    return ret;
}

/**********************************************************/
static Int4 fta_cmp_gene_syns(const TSynSet& syn1, const TSynSet& syn2)
{
    Int4 i = 0;

    TSynSet::const_iterator it1 = syn1.begin(),
                            it2 = syn2.begin();

    for(; it1 != syn1.end() && it2 != syn2.end(); ++it1, ++it2)
    {
        i = StringICmp(it1->c_str(), it2->c_str());
        if(i != 0)
            break;
    }

    if (it1 == syn1.end() && it2 == syn2.end())
        return(0);
    if (it1 == syn1.end())
        return(-1);
    if (it2 == syn2.end())
        return(1);
    return(i);
}

/**********************************************************/
static Int4 fta_cmp_locus_tags(const Char* lt1, const Char* lt2)
{
    if(lt1 == NULL && lt2 == NULL)
        return(0);
    if(lt1 == NULL)
        return(-1);
    if(lt2 == NULL)
        return(1);
    return(StringICmp(lt1, lt2));
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

    if (!grp1.IsSetLocus() && !grp2.IsSetLocus())
    {
        res = fta_cmp_gene_syns(syn1, syn2);
        if(res != 0)
            return(res);
        return(fta_cmp_locus_tags(grp1.IsSetLocus_tag() ? grp1.GetLocus_tag().c_str() : NULL,
                                  grp2.IsSetLocus_tag() ? grp2.GetLocus_tag().c_str() : NULL));
    }

    if (!grp1.IsSetLocus())
        return(-1);
    if (!grp2.IsSetLocus())
        return(1);

    res = StringICmp(grp1.GetLocus().c_str(), grp2.GetLocus().c_str());
    if(res != 0)
        return(res);

    res = fta_cmp_gene_syns(syn1, syn2);
    if(res != 0)
        return(res);

    return(fta_cmp_locus_tags(grp1.IsSetLocus_tag() ? grp1.GetLocus_tag().c_str() : NULL,
                              grp2.IsSetLocus_tag() ? grp2.GetLocus_tag().c_str() : NULL));
}

/**********************************************************/
static Int4 fta_cmp_locusyn(GeneListPtr glp1, GeneListPtr glp2)
{
    Int4 res;

    if(glp1 == NULL && glp2 == NULL)
        return(0);
    if(glp1 == NULL)
        return(-1);
    if(glp2 == NULL)
        return(1);

    if(glp1->locus.empty() && glp2->locus.empty())
    {
        res = fta_cmp_gene_syns(glp1->syn, glp2->syn);
        if(res != 0)
            return(res);
        return(fta_cmp_locus_tags(glp1->locus_tag.c_str(), glp2->locus_tag.c_str()));
    }
    if(glp1->locus.empty())
        return(-1);
    if(glp2->locus.empty())
        return(1);

    res = StringICmp(glp1->locus.c_str(), glp2->locus.c_str());
    if(res != 0)
        return(res);
    res = fta_cmp_gene_syns(glp1->syn, glp2->syn);
    if(res != 0)
        return(res);
    return(fta_cmp_locus_tags(glp1->locus_tag.c_str(), glp2->locus_tag.c_str()));
}

/**********************************************************/
static bool CompareGeneListName(const GeneListPtr& sp1, const GeneListPtr& sp2)
{
    SeqlocInfoblkPtr slip1 = sp1->slibp;
    SeqlocInfoblkPtr slip2 = sp2->slibp;

    Int4 status = sp1->segnum - sp2->segnum;
    if (status != 0)
        return status < 0;

    status = fta_cmp_locusyn(sp1, sp2);
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

/*static int LIBCALLBACK CompareGeneListName(void PNTR vp1, void PNTR vp2)
{
    GeneListPtr PNTR sp1 = (GeneListPtr PNTR) vp1;
    GeneListPtr PNTR sp2 = (GeneListPtr PNTR) vp2;
    SeqlocInfoblkPtr slip1;
    SeqlocInfoblkPtr slip2;
    Int4             status;

    slip1 = (*sp1)->slibp;
    slip2 = (*sp2)->slibp;

    status = (*sp1)->segnum - (*sp2)->segnum;
    if(status != 0)
        return(status);

    status = fta_cmp_locusyn(*sp1, *sp2);
    if(status != 0)
        return(status);

    status = slip1->strand - slip2->strand;
    if(status != 0)
        return(status);

    status = (Int4) ((*sp1)->leave - (*sp2)->leave);
    if(status != 0)
        return(status);

    status = slip1->from - slip2->from;
    if(status != 0)
        return(status);

    status = (Int4) (slip2->noleft - slip1->noleft);
    if(status != 0)
        return(status);

    status = slip1->to - slip2->to;
    if(status != 0)
        return(status);

    return((Int4) (slip1->noright - slip2->noright));
}*/

/**********************************************************/
static GeneNodePtr sort_gnp_list(GeneNodePtr gnp)
{
    Int4             index;
    Int4             total;
    GeneListPtr      glp;
    GeneListPtr* temp;

    total = 0;
    for(glp = gnp->glp; glp != NULL; glp = glp->next)
        total++;

    temp = (GeneListPtr*) MemNew(total * sizeof(GeneListPtr));

    for(index = 0, glp = gnp->glp; glp != NULL; glp = glp->next)
        temp[index++] = glp;

    std::sort(temp, temp + index, CompareGeneListName);

    gnp->glp = glp = temp[0];
    for(index = 0; index < total - 1; glp = glp->next, index++)
        glp->next = temp[index+1];

    glp = temp[total-1];
    glp->next = NULL;

    temp = (GeneListPtr*) MemFree(temp);

    return(gnp);
}


/**********************************************************/
static void MixLocFree(MixLocPtr mlp)
{
    MixLocPtr next;

    for(; mlp != NULL; mlp = next)
    {
        next = mlp->next;
        delete mlp;
    }
}

/**********************************************************/
static void GeneListFree(GeneListPtr glp)
{
    GeneListPtr glpnext;

    for(; glp != NULL; glp = glpnext)
    {
        glpnext = glp->next;
        glp->next = NULL;

       // if(glp->locus != NULL)
       //     MemFree(glp->locus);

       // if(glp->locus_tag != NULL)
       //     MemFree(glp->locus_tag);

       // if(glp->pseudogene != NULL)
       //     MemFree(glp->pseudogene);

       // if(glp->maploc != NULL)
       //     MemFree(glp->maploc);

        if(glp->slibp) {
            delete glp->slibp;
        }
        if(glp->mlp != NULL)
            MixLocFree(glp->mlp);

       // if(glp->fname != NULL)
       //     MemFree(glp->fname);

       // if(glp->location != NULL)
       //     MemFree(glp->location);

        delete glp;
    }
}

/**********************************************************/
static void CdssListFree(CdssListPtr clp)
{
    CdssListPtr clpnext;

    for(; clp != NULL; clp = clpnext)
    {
        clpnext = clp->next;
        delete clp;
    }
}


/**********************************************************/
static void GeneLocsFree(GeneLocsPtr gelop)
{
    GeneLocsPtr gelopnext;

    for(; gelop != NULL; gelop = gelopnext)
    {
        gelopnext = gelop->next;
        delete gelop;
    }
}

/**********************************************************
 *
 *   GetLowHighFromSeqLoc:
 *   -- get the lowest "from", highest "to" value, strand
 *      value within one feature key
 *
 **********************************************************/
static SeqlocInfoblkPtr GetLowHighFromSeqLoc(const CSeq_loc* origslp, Int4 length,
                                             const CSeq_id& orig_id)
{
    SeqlocInfoblkPtr slibp = NULL;

    Int4             from;
    Int4             to;

    Uint1            strand;

    bool          noleft;
    bool          noright;

    if (origslp != nullptr)
    {
        for (CSeq_loc_CI loc(*origslp); loc; ++loc)
        {
            noleft = false;
            noright = false;

            CConstRef<CSeq_loc> cur_loc = loc.GetRangeAsSeq_loc();
            const CSeq_id* id = nullptr;

            if (cur_loc->IsInt())
            {
                const CSeq_interval& interval = cur_loc->GetInt();
                id = &interval.GetId();

                from = interval.GetFrom();
                to = interval.GetTo();
                strand = interval.IsSetStrand() ? interval.GetStrand() : 0;

                if (interval.IsSetFuzz_from() && interval.GetFuzz_from().IsLim() &&
                    interval.GetFuzz_from().GetLim() == CInt_fuzz::eLim_lt)
                    noleft = true;
                if (interval.IsSetFuzz_to() && interval.GetFuzz_to().IsLim() &&
                    interval.GetFuzz_to().GetLim() == CInt_fuzz::eLim_gt)
                    noright = true;
            }
            else if (cur_loc->IsPnt())
            {
                const CSeq_point& point = cur_loc->GetPnt();
                id = &point.GetId();
                from = point.GetPoint();
                to = from;
                strand = point.IsSetStrand() ? point.GetStrand() : 0;

                if (point.IsSetFuzz() && point.GetFuzz().IsLim())
                {
                    if (point.GetFuzz().GetLim() == CInt_fuzz::eLim_gt)
                        noright = true;
                    else if (point.GetFuzz().GetLim() == CInt_fuzz::eLim_lt)
                        noleft = true;
                }
            }
            else
                continue;

            /* get low and high only for locations from the same entry
                */
            if (from < 0 || to < 0 || id == nullptr || orig_id.Compare(*id) == CSeq_id::e_NO)
                continue;

            if (slibp == NULL)
            {
                slibp = new SeqlocInfoblk;
                slibp->from = from;
                slibp->to = to;
                slibp->noleft = noleft;
                slibp->noright = noright;
                if (length != -99)
                {
                    slibp->strand = strand;

                    CRef<CSeq_id> sid(new CSeq_id);
                    sid->Assign(*id);
                    slibp->ids.push_back(sid);
                    slibp->length = length;     /* total bsp of the entry */
                }
            }
            else
            {
                if (slibp->from > from)
                {
                    slibp->from = from;
                    slibp->noleft = noleft;
                }
                if (slibp->to < to)
                {
                    slibp->to = to;
                    slibp->noright = noright;
                }
            }
        }
    }

    return(slibp);
}

/**********************************************************/
static bool DoWeHaveGeneInBetween(GeneListPtr c, SeqlocInfoblkPtr second, GeneNodePtr gnp)
{
    SeqlocInfoblkPtr slp;
    SeqlocInfoblkPtr first;
    GeneListPtr      glp;

    first = c->slibp;
    if(gnp == NULL || first->to >= second->from)
        return false;

    for(glp = gnp->glp; glp != NULL; glp = glp->next)
    {
        if(c->segnum != glp->segnum || glp->slibp == NULL)
            continue;

        slp = glp->slibp;
        if(gnp->skipdiv == false)
        {
            if(glp->leave != 0 || slp->strand != first->strand ||
               slp->from < first->from || slp->to > second->to ||
               fta_cmp_locusyn(glp, c) == 0)
                continue;
            break;
        }

        if(slp->to <= first->to || slp->from >= second->from ||
           slp->strand != first->strand || fta_cmp_locusyn(glp, c) == 0)
            continue;
        break;
    }

    return glp != NULL;
}

/**********************************************************/
static bool DoWeHaveCdssInBetween(GeneListPtr c, Int4 to, CdssListPtr clp)
{
    SeqlocInfoblkPtr cloc;

    cloc = c->slibp;
    if(cloc->to >= to)
        return false;
    for(; clp != NULL; clp = clp->next)
        if(c->segnum == clp->segnum && clp->from > cloc->to && clp->to < to)
            break;

    return clp != NULL;
}

/**********************************************************/
static void AddGeneFeat(GeneListPtr glp, const string& maploc, TSeqFeatList& feats)
{
    CRef<CSeq_feat> feat(new CSeq_feat);
    CGene_ref& gene_ref = feat->SetData().SetGene();

    if(!glp->locus.empty()) {
        gene_ref.SetLocus(glp->locus);
    }
    if(!glp->locus_tag.empty()) {
        gene_ref.SetLocus_tag(glp->locus_tag);
    }
    if (!maploc.empty()) {
        gene_ref.SetMaploc(maploc);
    }

    if (!glp->syn.empty())
    {
        gene_ref.SetSyn().assign(glp->syn.begin(), glp->syn.end());
        glp->syn.clear();
    }

    if (glp->loc.NotEmpty())
        feat->SetLocation(*glp->loc);

    if (glp->pseudo)
        feat->SetPseudo(true);

    if(glp->allpseudo)
        feat->SetPseudo(true);

    if(!glp->pseudogene.empty() && glp->pseudogene[0] != '\0')
    {
        CRef<CGb_qual> qual(new CGb_qual);
        qual->SetQual("pseudogene");
        qual->SetVal(glp->pseudogene);

        feat->SetQual().push_back(qual);
        feat->SetPseudo(true);
    }

    if (!glp->wormbase.empty())
    {
        if (glp->wormbase.size() > 1)
            ErrPostEx(SEV_WARNING, ERR_FEATURE_MultipleWBGeneXrefs,
                      "Multiple WormBase WBGene /db_xref qualifiers found for feature with Gene Symbol \"%s\" and Locus Tag \"%s\".",
                      (glp->locus.empty()) ? "NONE" : glp->locus.c_str(),
                      (glp->locus_tag.empty()) ? "NONE" : glp->locus_tag.c_str());

        for (TWormbaseSet::const_iterator it = glp->wormbase.begin(); it != glp->wormbase.end(); ++it)
        {
            if (it->empty())
                continue;

            CRef<CDbtag> tag(new CDbtag);

            tag->SetDb("WormBase");
            tag->SetTag().SetStr(*it);

            feat->SetDbxref().push_back(tag);
        }
    }

    if (!glp->olt.empty())
    {
        if (glp->olt.size() > 1)
            ErrPostEx(SEV_WARNING, ERR_FEATURE_MultipleOldLocusTags,
                      "Multiple /old_locus_tag qualifiers found for feature with Gene Symbol \"%s\" and Locus Tag \"%s\".",
                      (glp->locus.empty()) ? "NONE" : glp->locus.c_str(),
                      (glp->locus_tag.empty()) ? "NONE" : glp->locus_tag.c_str());

        for (TLocusTagSet::const_iterator it = glp->olt.begin(); it != glp->olt.end(); ++it)
        {
            if (it->empty())
                continue;

            CRef<CGb_qual> qual(new CGb_qual);
            qual->SetQual("old_locus_tag");
            qual->SetVal(*it);

            feat->SetQual().push_back(qual);
        }
    }

    glp->loc.Reset();
    feats.push_back(feat);
}

/**********************************************************/
static MixLocPtr MixLocCopy(MixLocPtr mlp)
{
    MixLocPtr res = new MixLoc();
    res->pId = mlp->pId;
    res->min = mlp->min;
    res->max = mlp->max;
    res->strand = mlp->strand;
    res->noleft = mlp->noleft;
    res->noright = mlp->noright;
    res->numint = mlp->numint;
    res->next = NULL;
    return res;
}
/**********************************************************/

static bool s_IdsMatch(const CRef<CSeq_id>& pId1, const CRef<CSeq_id>& pId2)
{
    if (!pId1 && !pId2) {
        return true;
    }

    if (!pId1 || !pId2) {
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

    if(first == NULL && second == NULL)
        return(NULL);

    tres = (MixLocPtr) MemNew(sizeof(MixLoc));
    res = tres;
    mlp = (first == NULL) ? second : first;
    for(; mlp != NULL; mlp = mlp->next)
    {
        res->next = MixLocCopy(mlp);
        res = res->next;
    }
    if(first != NULL && second != NULL)
    {
        for(mlp = second; mlp != NULL; mlp = mlp->next)
        {
            next = MixLocCopy(mlp);
            for(res = tres->next; res != NULL; res = res->next)
            {
                if(!s_IdsMatch(res->pId, next->pId) ||
                   res->strand != next->strand)
                    continue;

                ttt = res->next;
                res->next = next;
                next->next = ttt;
                break;
            }
            if(res != NULL)
                continue;

            for(prev = tres; prev->next != NULL; prev = prev->next)
            {
                ttt = prev->next;
                for(res = mlp->next; res != NULL; res = res->next)
                {
                    if(s_IdsMatch(res->pId, ttt->pId) && 
                       res->strand == ttt->strand)
                        break;
                }
                if(res != NULL)
                    break;
            }
            ttt = prev->next;
            prev->next = next;
            next->next = ttt;
        }
    }

    res = tres->next;
    MemFree(tres);
    if(res == NULL)
        return(NULL);

    for(got = 1; got == 1;)
    {
        got = 0;
        for(tres = res; tres != NULL; tres = tres->next)
        {
            if(!tres->pId)
                continue;
            for(mlp = tres->next; mlp != NULL; mlp = mlp->next)
            {
                if(!mlp->pId || 
                   !s_IdsMatch(tres->pId, mlp->pId) || 
                   tres->strand != mlp->strand)
                    continue;

                if(tres->min == mlp->min && tres->max == mlp->max)
                {
                    mlp->pId.Reset();
                    if(tres->noleft == false)
                        tres->noleft = mlp->noleft;
                    if(tres->noright == false)
                        tres->noright = mlp->noright;
                    got = 1;
                    continue;
                }

                if(join == false ||
                   (tres->min <= mlp->max + 1 && tres->max + 1 >= mlp->min))
                {
                    if(tres->min == mlp->min)
                    {
                        if(tres->noleft == false)
                            tres->noleft = mlp->noleft;
                    }
                    else if(tres->min > mlp->min)
                    {
                        tres->min = mlp->min;
                        tres->noleft = mlp->noleft;
                    }

                    if(tres->max == mlp->max)
                    {
                        if(tres->noright == false)
                            tres->noright = mlp->noright;
                    }
                    else if(tres->max < mlp->max)
                    {
                        tres->max = mlp->max;
                        tres->noright = mlp->noright;
                    }
                    mlp->pId.Reset();
                    got = 1;
                }
            }
        }
    }
    for(mlp = NULL, tres = res; tres != NULL; tres = next)
    {
        next = tres->next;
        if(tres->pId)
        {
            mlp = tres;
            continue;
        }
        if(mlp == NULL)
            res = tres->next;
        else
            mlp->next = tres->next;
        tres->next = NULL;
        MixLocFree(tres);
    }
    return(res);
}

/**********************************************************/
static MixLocPtr CircularSeqLocCollect(MixLocPtr first, MixLocPtr second)
{
    if (first == NULL && second == NULL)
        return(NULL);

    MixLocPtr tres = (MixLocPtr)MemNew(sizeof(MixLoc)),
              res = tres;
    for (MixLocPtr mlp = first; mlp != NULL; mlp = mlp->next) {
        res->next = MixLocCopy(mlp);
        res = res->next;
    }
    for (MixLocPtr mlp = second; mlp != NULL; mlp = mlp->next) {
        res->next = MixLocCopy(mlp);
        res = res->next;
    }

    res = tres->next;
    MemFree(tres);
    return(res);
}

/**********************************************************/
static void fta_add_wormbase(GeneListPtr fromglp, GeneListPtr toglp)
{
    toglp->wormbase.insert(fromglp->wormbase.begin(), fromglp->wormbase.end());
    fromglp->wormbase.clear();
}

/**********************************************************/
static void fta_add_olt(GeneListPtr fromglp, GeneListPtr toglp)
{
    toglp->olt.insert(fromglp->olt.begin(), fromglp->olt.end());
    fromglp->olt.clear();
}

/**********************************************************/
static void fta_check_pseudogene(GeneListPtr tglp, GeneListPtr glp)
{
    if(tglp == NULL || glp == NULL)
        return;

    if(tglp->pseudogene.empty() && glp->pseudogene.empty() )
        return;

    if(!tglp->pseudogene.empty() && !glp->pseudogene.empty())
    {
        if(tglp->pseudogene[0] == '\0' || glp->pseudogene[0] == '\0')
        {
            tglp->pseudogene[0] = '\0';
            glp->pseudogene[0] = '\0';
        }
        else if(tglp->pseudogene != glp->pseudogene)
        {
            ErrPostEx(SEV_ERROR, ERR_FEATURE_InconsistentPseudogene,
                      "All /pseudogene qualifiers for a given Gene and/or Locus-Tag should be uniform. But pseudogenes \"%s\" vs. \"%s\" exist for the features with Gene Symbol \"%s\" and Locus Tag \"%s\".",
                      (glp->locus.empty()) ? "NONE" : glp->locus.c_str(),
                      (glp->locus_tag.empty()) ? "NONE" : glp->locus_tag.c_str(),
                      tglp->pseudogene.c_str(), glp->pseudogene.c_str());
            tglp->pseudogene[0] = '\0';
            glp->pseudogene[0] = '\0';
        }
        return;
    }

    if(tglp->pseudogene.empty())
    {
        if(glp->pseudogene[0] != '\0')
            tglp->pseudogene = glp->pseudogene;
        else
        {
            tglp->pseudogene.resize(1);
            tglp->pseudogene[0] = '\0';
        }
    }
    else if(glp->pseudogene.empty())
    {
        if(tglp->pseudogene[0] != '\0')
            glp->pseudogene = tglp->pseudogene;
        else
        {
            glp->pseudogene.resize(1);
            glp->pseudogene[0] = '\0';
        }
    }
}

/**********************************************************/
static void MessWithSegGenes(GeneNodePtr gnp)
{
    GeneListPtr glp;
    GeneListPtr tglp;
    GeneListPtr next;
    GeneListPtr prev;
    MixLocPtr   mlp;
    MixLocPtr   tmlp;
    Int4        segnum;
    Int4        i;
    Uint1       strand;

    for(glp = gnp->glp; glp != NULL && glp->segnum == 1; glp = glp->next)
    {
        if(glp->loc != NULL || glp->mlp == NULL)
            continue;
        segnum = 1;
        strand = glp->slibp->strand;
        for(tglp = gnp->glp; tglp != NULL; tglp = tglp->next)
        {
            if(tglp->loc != NULL || glp->mlp == NULL ||
               fta_cmp_locusyn(glp, tglp) != 0)
                continue;

            i = tglp->segnum - segnum;
            if(i < 0 || i > 1)
                break;

            if(tglp->slibp->strand != strand)
                continue;

            segnum = tglp->segnum;
        }
        if(segnum != gnp->segindex)
            continue;

        segnum = 0;
        mlp = NULL;
        for(tglp = gnp->glp; tglp != NULL; tglp = tglp->next)
        {
            if(tglp->loc != NULL || tglp->segnum - segnum != 1 ||
               tglp->mlp == NULL || fta_cmp_locusyn(glp, tglp) != 0)
                continue;

            if(tglp->slibp->strand != strand)
                continue;
            segnum++;
            tmlp = EasySeqLocMerge(mlp, tglp->mlp, false);
            MixLocFree(tglp->mlp);
            MixLocFree(mlp);
            mlp = tmlp;

            if(segnum != gnp->segindex)
            {
                tglp->mlp = NULL;
                if(tglp->pseudo)
                    glp->pseudo = true;
                if(tglp->allpseudo == false)
                    glp->allpseudo = false;
                fta_check_pseudogene(tglp, glp);
                fta_add_wormbase(tglp, glp);
                fta_add_olt(tglp, glp);
                continue;
            }
            tglp->mlp = mlp;
            break;
        }
    }
    prev = NULL;
    for(tglp = gnp->glp; tglp != NULL; tglp = next)
    {
        next = tglp->next;
        if(tglp->mlp != NULL)
        {
            prev = tglp;
            continue;
        }
        if(prev == NULL)
            gnp->glp = tglp->next;
        else
            prev->next = tglp->next;
        tglp->next = NULL;
        GeneListFree(tglp);
    }
}

/**********************************************************/
static bool fta_check_feat_overlap(GeneLocsPtr gelop, GeneListPtr c,
                                   MixLocPtr mlp, Int4 from, Int4 to)
{
    Int4         min;
    Int4         max;

    if(gelop == NULL || c == NULL || mlp == NULL)
        return true;

    min = (mlp->min > from) ? from : mlp->min;
    max = (mlp->max < to) ? to : mlp->max;

    for(; gelop != NULL; gelop = gelop->next)
    {
        if(min > gelop->verymax)
        {
            gelop = NULL;
            break;
        }

        if((gelop->strand > -1 && c->slibp->strand != gelop->strand) ||
           max < gelop->verymin)
            continue;

        if(fta_strings_same(c->locus.c_str(), gelop->gene.c_str()) && fta_strings_same(c->locus_tag.c_str(), gelop->locus.c_str()))
            continue;
        auto it = gelop->ammp.begin();
        for (; it != gelop->ammp.end(); ++it) {
            auto ammp = *it;
            int ver1=0;
            string label1;
            ammp.pId->GetLabel(&label1, &ver1);
            string label2;
            int ver2=0;
            mlp->pId->GetLabel(&label2, &ver2);
            if(max < ammp.min || min > ammp.max || ver1 != ver2)
                continue;
            if(label1 == label2)
                break;
        }
        if (it != gelop->ammp.end()) {
            break;
        }
    }

    return gelop != NULL;
}

/**********************************************************/
static bool ConfirmCircular(MixLocPtr mlp)
{
    MixLocPtr tmlp;

    if (mlp == NULL || mlp->next == NULL)
        return false;

    tmlp = mlp;
    if (mlp->strand != 2) {
        for (; tmlp->next != NULL; tmlp = tmlp->next)
            if (tmlp->min > tmlp->next->min)
                break;
    }
    else {
        for (; tmlp->next != NULL; tmlp = tmlp->next)
            if (tmlp->min < tmlp->next->min)
                break;
    }

    if (tmlp->next != NULL)
        return true;

    for (tmlp = mlp; tmlp != NULL; tmlp = tmlp->next) {
        tmlp->numint = 0;
    }
    return false;
}

/**********************************************************/
static void FixMixLoc(GeneListPtr c, GeneLocsPtr gelop)
{
    Int4         from;
    Int4         to;
    MixLocPtr    mlp;
    MixLocPtr    tmlp;
    Uint1        strand;

    bool      noleft;
    bool      noright;
    bool      tempcirc;

    c->mlp = NULL;

    if (c->feat.Empty() || !c->feat->IsSetLocation())
    {
       CRef<CSeq_id> pTempId;
       for (auto pId : c->slibp->ids) {
           if (pId) {
                pTempId = pId;
                break;
           }
       }

       if (!pTempId) {
           return;
       }
        
        mlp = new MixLoc();
        mlp->pId = pTempId;
        mlp->min = c->slibp->from;
        mlp->max = c->slibp->to;
        mlp->strand = c->slibp->strand;
        mlp->noleft = c->slibp->noleft;
        mlp->noright = c->slibp->noright;
        mlp->numint = 0;
        c->mlp = mlp;
        return;
    }

    if(c->leave == 1)
    {
        c->loc.Reset(&c->feat->SetLocation());
        return;
    }

    mlp = NULL;
    Int4 i = 1;
    const CSeq_loc& locs = c->feat->GetLocation();
    ITERATE(CSeq_loc, loc, locs)
    {
        noleft = false;
        noright = false;
        CRef<CSeq_id> pId;

        CConstRef<CSeq_loc> cur_loc = loc.GetRangeAsSeq_loc();
        if (cur_loc->IsInt())
        {
            const CSeq_interval& interval = cur_loc->GetInt();
            if (interval.IsSetId()) {
                pId = Ref(new CSeq_id());
                pId->Assign(interval.GetId());
            }

            from = interval.IsSetFrom() ? interval.GetFrom() : 0;
            to = interval.IsSetTo() ? interval.GetTo() : 0;
            strand = cur_loc->IsSetStrand() ? cur_loc->GetStrand() : 0;

            if (interval.IsSetFuzz_from() && interval.GetFuzz_from().IsLim() && interval.GetFuzz_from().GetLim() == CInt_fuzz::eLim_lt)
                noleft = true;

            if (interval.IsSetFuzz_to() && interval.GetFuzz_to().IsLim() && interval.GetFuzz_to().GetLim() == CInt_fuzz::eLim_gt)
                noright = true;
        }
        else if (cur_loc->IsPnt())
        {
            const CSeq_point& point = cur_loc->GetPnt();
            if (point.IsSetId()) {
                pId = Ref(new CSeq_id());
                pId->Assign(point.GetId());
             }

            from = point.IsSetPoint() ? point.GetPoint() : 0;
            to = from;
            strand = cur_loc->IsSetStrand() ? cur_loc->GetStrand() : 0;

            if (point.IsSetFuzz() && point.GetFuzz().IsLim())
            {
                if (point.GetFuzz().GetLim() == CInt_fuzz::eLim_gt)
                    noright = true;
                else if (point.GetFuzz().GetLim() == CInt_fuzz::eLim_lt)
                    noleft = true;
            }
        }
        else
            continue;

        if (!pId || from < 0 || to < 0) {
            continue;
        }


        if (mlp == NULL)
        {
            mlp = new MixLoc();
            mlp->pId = pId;
            mlp->min = from;
            mlp->max = to;
            mlp->strand = strand;
            mlp->noleft = noleft;
            mlp->noright = noright;
            mlp->numint = i++;
            continue;
        }

        for (tmlp = mlp; ;tmlp = tmlp->next)
        {
            tempcirc = false;
            if (s_IdsMatch(pId, tmlp->pId) && tmlp->strand == strand)
            {
                if (tempcirc == false && ((tmlp->min <= to && tmlp->max >= from) ||
                    fta_check_feat_overlap(gelop, c, tmlp, from, to) == false))
                {
                    if (tmlp->min > from)
                    {
                        tmlp->min = from;
                        tmlp->noleft = noleft;
                    }
                    if (tmlp->max < to)
                    {
                        tmlp->max = to;
                        tmlp->noright = noright;
                    }
                    break;
                }
            }

            if (tmlp->next != NULL)
                continue;

            tmlp->next = new MixLoc();
            tmlp = tmlp->next;
            tmlp->pId = pId;
            tmlp->min = from;
            tmlp->max = to;
            tmlp->strand = strand;
            tmlp->noleft = noleft;
            tmlp->noright = noright;
            tmlp->numint = i++;
            break;
        }
    }

    c->mlp = mlp;
}


/**********************************************************/
static void fta_make_seq_int(MixLocPtr mlp, bool noleft, bool noright, CSeq_interval& interval)
{
    if (mlp->strand != 0)
        interval.SetStrand(static_cast<CSeq_point::TStrand>(mlp->strand));

    interval.SetFrom(mlp->min);
    interval.SetTo(mlp->max);
    
    interval.SetId(*(mlp->pId));

    if(mlp->noleft || noleft)
    {
        interval.SetFuzz_from().SetLim(CInt_fuzz::eLim_lt);
    }

    if(mlp->noright || noright)
    {
        interval.SetFuzz_to().SetLim(CInt_fuzz::eLim_gt);
    }
}

/**********************************************************/
static void fta_make_seq_pnt(MixLocPtr mlp, bool noleft, bool noright, CSeq_point& point)
{
    if (mlp->strand != 0)
        point.SetStrand(static_cast<CSeq_point::TStrand>(mlp->strand));
    point.SetPoint(mlp->min);

    point.SetId(*(mlp->pId));

    if(mlp->noleft || mlp->noright || noleft || noright)
    {
        CInt_fuzz::TLim lim = (mlp->noleft == false && noleft == false) ? CInt_fuzz::eLim_gt : CInt_fuzz::eLim_lt;
        point.SetFuzz().SetLim(lim);
    }
}

/**********************************************************/
static CRef<CSeq_loc> MakeCLoc(MixLocPtr mlp, bool noleft, bool noright)
{
    CRef<CSeq_loc> ret(new CSeq_loc);

    if (mlp->next == NULL)
    {
        if(mlp->min == mlp->max)
            fta_make_seq_pnt(mlp, noleft, noright, ret->SetPnt());
        else
            fta_make_seq_int(mlp, noleft, noright, ret->SetInt());
        return ret;
    }

    CRef<CSeq_loc> cur;
    CSeq_loc_mix& mix = ret->SetMix();

    for(; mlp != NULL; mlp = mlp->next)
    {
        cur.Reset(new CSeq_loc);

        if (mlp->min == mlp->max)
        {
            fta_make_seq_pnt(mlp, noleft, noright, cur->SetPnt());
        }
        else
        {
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
    for(mlp = second; mlp != NULL; mlp = mlp->next)
    {
        if(!mlp->pId)
            continue;
        for(tmlp = second; tmlp < mlp; tmlp = tmlp->next)
            if(tmlp->pId && s_IdsMatch(tmlp->pId, mlp->pId))
                break;
        if(tmlp < mlp)
            continue;
        count++;
    }
    for(mlp = first; mlp != NULL; mlp = mlp->next)
    {
        if(!mlp->pId)
            continue;
        for(tmlp = first; tmlp < mlp; tmlp = tmlp->next)
            if(tmlp->pId && s_IdsMatch(tmlp->pId, mlp->pId))
                break;
        if(tmlp < mlp)
            continue;
        count--;
    }
    return(count);
}

/**********************************************************/
static void CircularSeqLocFormat(GeneListPtr c)
{
    MixLocPtr mlp;
    MixLocPtr tmlp;
    MixLocPtr mlpprev;
    MixLocPtr mlpnext;

    if(c->mlp == NULL || c->mlp->next == NULL)
        return;

    for(bool got = true; got == true;)
    {
        got = false;
        for(mlp = c->mlp; mlp != NULL; mlp = mlp->next)
        {
            if(mlp->numint == -1)
                continue;
            for(tmlp = mlp->next; tmlp != NULL; tmlp = tmlp->next)
            {
                if(tmlp->numint == -1 || mlp->strand != tmlp->strand)
                    continue;

                if(mlp->numint == 0 && tmlp->numint == 0)
                {
                    if((tmlp->min >= mlp->min && tmlp->min <= mlp->max) ||
                       (tmlp->max >= mlp->min && tmlp->max <= mlp->max) ||
                       (mlp->min > tmlp->min && mlp->max < tmlp->max))
                    {
                        if(mlp->min == tmlp->min)
                        {
                            if(tmlp->noleft)
                                mlp->noleft = true;
                        }
                        else if(mlp->min > tmlp->min)
                        {
                            mlp->min = tmlp->min;
                            mlp->noleft = tmlp->noleft;
                        }
                        if(mlp->max == tmlp->max)
                        {
                            if(tmlp->noright)
                                mlp->noright = true;
                        }
                        else if(mlp->max < tmlp->max)
                        {
                            mlp->max = tmlp->max;
                            mlp->noright = tmlp->noright;
                        }
                        tmlp->numint = -1;
                        got = true;
                    }
                    continue;
                }

                if(mlp->min != tmlp->min || mlp->max != tmlp->max)
                    continue;
                if(tmlp->numint == 0)
                {
                    if(tmlp->noleft)
                        mlp->noleft = true;
                    if(tmlp->noright)
                        mlp->noright = true;
                    tmlp->numint = -1;
                }
                else if(mlp->numint == 0)
                {
                    if(mlp->noleft)
                        tmlp->noleft = true;
                    if(mlp->noright)
                        tmlp->noright = true;
                    mlp->numint = -1;
                }
                else if(tmlp->numint >= mlp->numint)
                {
                    if(tmlp->noleft)
                        mlp->noleft = true;
                    if(tmlp->noright)
                        mlp->noright = true;
                    tmlp->numint = -1;
                }
                else
                {
                    if(mlp->noleft)
                        tmlp->noleft = true;
                    if(mlp->noright)
                        tmlp->noright = true;
                    mlp->numint = -1;
                }
                got = true;
            }
        }
    }

    for(mlpprev = NULL, mlp = c->mlp; mlp != NULL; mlp = mlpnext)
    {
        mlpnext = mlp->next;
        if(mlp->numint != -1)
        {
            mlpprev = mlp;
            continue;
        }

        if(mlpprev == NULL)
            c->mlp = mlpnext;
        else
            mlpprev->next = mlpnext;
        mlp->next = NULL;
        MixLocFree(mlp);
    }

    mlpprev = NULL;
    for(mlp = c->mlp; mlp != NULL; mlpprev = mlp, mlp = mlp->next)
        if(mlp->numint == 1)
            break;

    if(mlp != NULL && mlp != c->mlp)
    {
        mlpprev->next = NULL;
        for(tmlp = mlp; tmlp->next != NULL;)
            tmlp = tmlp->next;
        tmlp->next = c->mlp;
        c->mlp = mlp;
    }
}

/**********************************************************/
static void SortMixLoc(GeneListPtr c)
{
    MixLocPtr mlp;
    MixLocPtr tmlp;
    
    bool   noleft;
    bool   noright;

    Int4      min;
    Int4      max;
    Int4      numint;

    if(c->mlp == NULL || c->mlp->next == NULL)
        return;

    for(mlp = c->mlp; mlp != NULL; mlp = mlp->next)
    {
        for(tmlp = mlp->next; tmlp != NULL; tmlp = tmlp->next)
        {
            if(!s_IdsMatch(mlp->pId, tmlp->pId) ||
               mlp->strand != tmlp->strand)
                break;
            if(mlp->strand == 2)
            {
                if(tmlp->min < mlp->min)
                    continue;
                if(tmlp->min == mlp->min)
                {
                    if(tmlp->noleft == mlp->noleft)
                    {
                        if(tmlp->max < mlp->max)
                            continue;
                        if(tmlp->max == mlp->max)
                        {
                            if(tmlp->noright == mlp->noright || mlp->noright)
                                continue;
                        }
                    }
                    else if (mlp->noleft)
                        continue;
                }
            }
            else
            {
                if(tmlp->min > mlp->min)
                    continue;
                if(tmlp->min == mlp->min)
                {
                    if(tmlp->noleft == mlp->noleft)
                    {
                        if(tmlp->max > mlp->max)
                            continue;
                        if(tmlp->max == mlp->max)
                        {
                            if(tmlp->noright == mlp->noright || tmlp->noright)
                                continue;
                        }
                    }
                    else if(tmlp->noleft)
                        continue;
                }
            }
            min = mlp->min;
            max = mlp->max;
            noleft = mlp->noleft;
            noright = mlp->noright;
            numint = mlp->numint;
            mlp->min = tmlp->min;
            mlp->max = tmlp->max;
            mlp->noleft = tmlp->noleft;
            mlp->noright = tmlp->noright;
            mlp->numint = tmlp->numint;
            tmlp->min = min;
            tmlp->max = max;
            tmlp->noleft = noleft;
            tmlp->noright = noright;
            tmlp->numint = numint;
        }
    }
}

/**********************************************************/
static void ScannGeneName(GeneNodePtr gnp, Int4 seqlen)
{
    GeneListPtr c;
    GeneListPtr cn;
    GeneListPtr cp;

    MixLocPtr   mlp;
    Int4        j;
    Int2        level;

    bool     join;

    for(c = gnp->glp; c->next != NULL; c = c->next)
    {
        for(cn = c->next; cn != NULL; cn = cn->next)
        {
            if(c->segnum == cn->segnum &&
               c->feat.NotEmpty() && cn->feat.NotEmpty() &&
               c->feat->IsSetData() && c->feat->GetData().IsCdregion() &&
               cn->feat->IsSetData() && c->feat->GetData().IsCdregion() &&
               fta_cmp_locusyn(c, cn) == 0)
            {
                ErrPostEx(SEV_WARNING, ERR_GENEREF_NoUniqMaploc,
                          "Two different cdregions for one gene %s\"%s\".",
                          (c->locus.empty()) ? "with locus_tag " : "",
                          (c->locus.empty()) ? c->locus_tag.c_str() : c->locus.c_str());
            }
        }
    }

    bool circular = false;
    for (j = 1, c = gnp->glp; c != NULL; c = c->next, j++) {

        FixMixLoc(c, gnp->gelop);
        if (gnp->circular && ConfirmCircular(c->mlp))
            circular = true;
    }

    gnp->circular = circular;


    for(c = gnp->glp; c->next != NULL;)
    {
        cn = c->next;
        if(c->segnum != cn->segnum || fta_cmp_locusyn(c, cn) != 0 ||
           c->leave != 0 || cn->leave != 0 ||
           c->slibp->strand != cn->slibp->strand ||
           (gnp->simple_genes == false && DoWeHaveGeneInBetween(c, cn->slibp, gnp)))
        {
            c = cn;
            continue;
        }

        if(gnp->skipdiv && gnp->clp != NULL && gnp->simple_genes == false &&
           DoWeHaveCdssInBetween(c, cn->slibp->from, gnp->clp))
        {
            c = cn;
            continue;
        }

        if(c->slibp->from > cn->slibp->from)
        {
            c->slibp->from = cn->slibp->from;
            c->slibp->noleft = cn->slibp->noleft;
        }
        if(c->slibp->to < cn->slibp->to)
        {
            c->slibp->to = cn->slibp->to;
            c->slibp->noright = cn->slibp->noright;
        }

        if (!gnp->simple_genes)
        {
            for(cp = gnp->glp; cp != NULL; cp = cp->next)
            {
                if(cp->segnum != c->segnum || cp->leave == 1 || cp->circular)
                    continue;
                if(fta_cmp_locusyn(cp, c) == 0 ||
                   cp->slibp->strand != c->slibp->strand)
                    continue;
                if(c->slibp->from <= cp->slibp->to &&
                   c->slibp->to >= cp->slibp->from)
                    break;
            }
            join = (cp != NULL);
        }
        else
            join = false;

        if (!gnp->circular)
        {
            level = GetMergeOrder(c->mlp, cn->mlp);
            if (level > 0)
            {
                mlp = EasySeqLocMerge(cn->mlp, c->mlp, join);
                c->feat = cn->feat;
            }
            else
                mlp = EasySeqLocMerge(c->mlp, cn->mlp, join);
        }
        else
            mlp = CircularSeqLocCollect(c->mlp, cn->mlp);

        if(c->mlp != NULL)
            MixLocFree(c->mlp);
        c->mlp = mlp;
        if(cn->pseudo)
            c->pseudo = true;
        if(!cn->allpseudo)
            c->allpseudo = false;
        fta_check_pseudogene(cn, c);
        fta_add_wormbase(cn, c);
        fta_add_olt(cn, c);
        c->noleft = c->slibp->noleft;
        c->noright = c->slibp->noright;
        c->next = cn->next;
        cn->next = NULL;
        GeneListFree(cn);
    }

    for (c = gnp->glp; c != NULL; c = c->next)
    {
        SortMixLoc(c);
        if(gnp->circular)
            CircularSeqLocFormat(c);
    }

    if (gnp->seg)
        MessWithSegGenes(gnp);

    for(c = gnp->glp; c != NULL; c = c->next)
        if(c->loc.Empty() && c->mlp != NULL)
            c->loc = MakeCLoc(c->mlp, c->noleft, c->noright);

    for (c = gnp->glp; c != NULL; c = c->next)
    {
        if(c->loc.Empty())
            continue;

        const CSeq_loc& loc = *c->loc;
        for(cn = gnp->glp; cn != NULL; cn = cn->next)
        {
            if (cn->loc.Empty() || &loc == cn->loc || cn->segnum != c->segnum ||
               cn->slibp->strand != c->slibp->strand ||
               fta_cmp_locusyn(cn, c) != 0)
                continue;

            sequence::ECompare cmp_res = sequence::Compare(loc, *cn->loc, nullptr, sequence::fCompareOverlapping);
            if (cmp_res != sequence::eContains && cmp_res != sequence::eSame)
                continue;

            if(cn->leave == 1 || c->leave == 0)
                c->leave = cn->leave;
            if(cn->pseudo)
                c->pseudo = true;
            if(!cn->allpseudo)
                c->allpseudo = false;
            fta_check_pseudogene(cn, c);
            fta_add_wormbase(cn, c);
            fta_add_olt(cn, c);
            if(cn->noleft)
                c->noleft = true;
            if(cn->noright)
                c->noright = true;
            
            cn->loc.Reset();
        }
    }

    for (cp = NULL, c = gnp->glp; c != NULL; c = cn)
    {
        cn = c->next;
        if(c->loc == NULL)
        {
            if(cp == NULL)
                gnp->glp = cn;
            else
                cp->next = cn;
            c->next = NULL;
            GeneListFree(c);
        }
        else
            cp = c;
    }

    for(c = gnp->glp; c != NULL; c = cn)
    {
        cn = c->next;
        if(cn == NULL || fta_cmp_locusyn(c, cn) != 0)
        {
            AddGeneFeat(c, c->maploc, gnp->feats);
            continue;
        }

        string maploc;
        for(cn = c; cn != NULL; cn = cn->next)
        {
            if(fta_cmp_locusyn(c, cn) != 0)
                break;
            if(cn->maploc.empty())
                continue;

            if(maploc.empty()) {
                maploc = cn->maploc;
            }
            else if(!NStr::EqualNocase(maploc, cn->maploc))
            {
                ErrPostEx(SEV_WARNING, ERR_GENEREF_NoUniqMaploc,
                          "Different maplocs in the gene %s\"%s\".",
                          (c->locus.empty()) ? "with locus_tag " : "",
                          (c->locus.empty()) ? c->locus_tag.c_str() : c->locus.c_str());
            }
        }
        for(cn = c; cn != NULL; cn = cn->next)
        {
            if(fta_cmp_locusyn(c, cn) != 0)
                break;
            AddGeneFeat(cn, maploc, gnp->feats);
        }
    }
}

/**********************************************************/
static CRef<CSeq_id> CpSeqIdAcOnly(const CSeq_id& id, bool accver)
{
    auto new_id = Ref(new CSeq_id());
    new_id->Assign(id);

    if (!accver) {
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
    const Char **b;
    short*    i;

    if (data.IsRna())
    {
        const CRNA_ref& rna_ref = data.GetRna();

        i = NULL;
        if (rna_ref.IsSetType())
        {
            for (i = leave_rna_feat; *i != -1; i++)
            {
                if (rna_ref.GetType() == *i)
                    break;
            }
        }

        if(i != NULL && *i != -1)
            return true;

        if (rna_ref.GetType() == CRNA_ref::eType_other && rna_ref.IsSetExt() && rna_ref.GetExt().IsName() &&
            rna_ref.GetExt().GetName() == "ncRNA")
            return true;
    }
    else if(data.IsImp())
    {
        b = NULL;
        if (data.GetImp().IsSetKey())
        {
            for (b = leave_imp_feat; *b != NULL; b++)
            {
                if (data.GetImp().GetKey() == *b)
                    break;
            }
        }
        if (b != NULL && *b != NULL)
            return true;
    }
    return false;
}

/**********************************************************/
static void GetGeneSyns(const TQualVector& quals, char* name, TSynSet& syns)
{
    if(name == NULL)
        return;

    ITERATE(TQualVector, qual, quals)
    {
        if (!(*qual)->IsSetQual() || !(*qual)->IsSetVal() ||
            (*qual)->GetQual() != "gene" ||
            StringICmp((*qual)->GetVal().c_str(), name) == 0)
            continue;

        syns.insert((*qual)->GetVal());
    }

    ITERATE(TQualVector, qual, quals)
    {
        if (!(*qual)->IsSetQual() || !(*qual)->IsSetVal() ||
            (*qual)->GetQual() != "gene_synonym" ||
            StringICmp((*qual)->GetVal().c_str(), name) == 0)
            continue;

        syns.insert((*qual)->GetVal());
    }
}

/**********************************************************/
static bool fta_rnas_cds_feat(const CSeq_feat& feat)
{
    if (!feat.IsSetData())
        return false;

    if (feat.GetData().IsImp())
    {
        if (feat.GetData().GetImp().IsSetKey())
        {
            const std::string& key = feat.GetData().GetImp().GetKey();
            if (key == "CDS" || key == "rRNA" ||
                key == "tRNA" || key == "mRNA")
                return true;
        }
        return false;
    }

    if (feat.GetData().IsCdregion())
        return true;

    if (!feat.GetData().IsRna())
        return false;

    const CRNA_ref& rna_ref = feat.GetData().GetRna();
    if (rna_ref.IsSetType() && rna_ref.GetType() > 1 && rna_ref.GetType() < 5)   /* mRNA, tRNA or rRNA */
        return true;

    return false;
}

/**********************************************************/
static bool IfCDSGeneFeat(const CSeq_feat& feat, Uint1 choice, const char *key)
{
    if (feat.IsSetData() && feat.GetData().Which() == choice)
        return true;

    if (feat.GetData().IsImp())
    {
        if (feat.GetData().GetImp().IsSetKey() && feat.GetData().GetImp().GetKey() == key)
            return true;
    }

    return false;
}

/**********************************************************/
static bool GetFeatNameAndLoc(GeneListPtr glp, const CSeq_feat& feat, GeneNodePtr gnp)
{
    const char *p;

    bool ret = false;

    p = NULL;
    if (feat.IsSetData())
    {
        if (feat.GetData().IsImp())
        {
            p = feat.GetData().GetImp().IsSetKey() ? feat.GetData().GetImp().GetKey().c_str() : NULL;
        }
        else if (feat.GetData().IsCdregion())
            p = "CDS";
        else if (feat.GetData().IsGene())
            p = "gene";
        else if (feat.GetData().IsBiosrc())
            p = "source";
        else if (feat.GetData().IsRna())
        {
            const CRNA_ref& rna_ref = feat.GetData().GetRna();

            if (rna_ref.IsSetType())
            {
                switch (rna_ref.GetType())
                {
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
            }
            else
                p = "an RNA";
        }
    }

    if(p == NULL)
        p = "a";

    ret = (MatchArrayString(feat_no_gene, p) < 0);

    if(glp == NULL)
        return(ret);

    if(StringCmp(p, "misc_feature") == 0)
       gnp->got_misc = true;

    glp->fname = p;

    if (!feat.IsSetLocation())
    {
        glp->location = "Unknown";
        return(ret);
    }

    string loc_str;
    GetLocationStr(feat.GetLocation(), loc_str);
    if (loc_str.empty())
        glp->location = "Unknown";
    else
    {
        if(loc_str.size() > 55)
        {
            loc_str = loc_str.substr(0, 50);
            loc_str += " ...";
        }
        glp->location = loc_str;
    }

    return(ret);
}

/**********************************************************/
static list<AccMinMax> fta_get_acc_minmax_strand(const CSeq_loc* location,
                                              GeneLocsPtr gelop)
{
    list<AccMinMax> ammps;
    const char   *acc;

    Int4         from;
    Int4         to;
    Int2         ver;

    gelop->strand = -2;

    if (location != nullptr)
    {
        for (CSeq_loc_CI loc(*location); loc; ++loc)
        {
            CConstRef<CSeq_loc> cur_loc = loc.GetRangeAsSeq_loc();
            CRef<CSeq_id> pId;
            if (cur_loc->IsInt())
            {
                const CSeq_interval& interval = cur_loc->GetInt();
                if (interval.IsSetId()) {
                    pId = Ref(new CSeq_id());
                    pId->Assign(interval.GetId());
                }
                from = interval.GetFrom();
                to = interval.GetTo();

                Int4 strand = interval.IsSetStrand() ? interval.GetStrand() : 0;
                if (gelop->strand == -2)
                    gelop->strand = strand;
                else if (gelop->strand != strand)
                    gelop->strand = -1;
            }
            else if (cur_loc->IsPnt())
            {
                const CSeq_point& point = cur_loc->GetPnt();
                if (point.IsSetId()) {
                    pId = Ref(new CSeq_id());
                    pId->Assign(point.GetId());
                }
                from = point.GetPoint();
                to = from;

                Int4 strand = point.IsSetStrand() ? point.GetStrand() : 0;
                if (gelop->strand == -2)
                    gelop->strand = strand;
                else if (gelop->strand != strand)
                    gelop->strand = -1;
            }
            else {
                continue;
            }

            _ASSERT(pId);

            if (gelop->verymin > from)
                gelop->verymin = from;
            if (gelop->verymax < to)
                gelop->verymax = to;

            bool found_id=false;
            auto it = ammps.begin();
            while (!found_id && it != ammps.end()) {
                auto ammp = *it;
                if (s_IdsMatch(ammp.pId, pId)) {
                    if (from < ammp.min) {
                        ammp.min = from;
                    }
                    if (to > ammp.max) {
                        ammp.max = to;
                    }
                    found_id=true;
                }
                ++it;
            }

            if (!found_id) {
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
static void fta_append_feat_list(GeneNodePtr gnp, const CSeq_loc* location,
                                 char* gene, char* locus_tag)
{

    if(gnp == NULL || location == NULL)
        return;

    GeneLocsPtr gelop = new GeneLocs();
    if (gene) {
        gelop->gene = gene;
    }
    else {
        gelop->gene.clear();
    }
    if (locus_tag) {
        gelop->locus = locus_tag;
    }
    else {
        gelop->locus.clear();
    }

    gelop->verymin = -1;
    gelop->verymax = -1;
    gelop->ammp = fta_get_acc_minmax_strand(location, gelop);
    gelop->next = gnp->gelop;
    gnp->gelop = gelop;
}

/**********************************************************/
static bool CompareGeneLocsMinMax(const GeneLocsPtr& sp1, const GeneLocsPtr& sp2)
{
    Int4 status = sp2->verymax - sp1->verymax;
    if (status == 0)
        status = sp1->verymin - sp2->verymin;

    return status < 0;
}

/**********************************************************/
static GeneLocsPtr fta_sort_feat_list(GeneLocsPtr gelop)
{
    Int4             index;
    Int4             total;
    GeneLocsPtr      glp;
    GeneLocsPtr* temp;

    total = 0;
    for(glp = gelop; glp != NULL; glp = glp->next)
        total++;

    temp = (GeneLocsPtr*) MemNew(total * sizeof(GeneLocsPtr));

    for(index = 0, glp = gelop; glp != NULL; glp = glp->next)
        temp[index++] = glp;

    std::sort(temp, temp + index, CompareGeneLocsMinMax);

    gelop = glp = temp[0];
    for(index = 0; index < total - 1; glp = glp->next, index++)
        glp->next = temp[index+1];

    glp = temp[total-1];
    glp->next = NULL;

    MemFree(temp);

    return(gelop);
}

/**********************************************************/
static void fta_collect_wormbases(GeneListPtr glp, CSeq_feat& feat)
{
    if(glp == NULL || !feat.IsSetDbxref())
        return;

    CSeq_feat::TDbxref dbxrefs;
    for (CSeq_feat::TDbxref::iterator dbxref = feat.SetDbxref().begin(); dbxref != feat.SetDbxref().end(); ++dbxref)
    {
        if (!(*dbxref)->IsSetTag() || !(*dbxref)->IsSetDb() ||
            (*dbxref)->GetDb() != "WormBase" ||
            StringNCmp((*dbxref)->GetTag().GetStr().c_str(), "WBGene", 6) != 0)
        {
            dbxrefs.push_back(*dbxref);
            continue;
        }

        glp->wormbase.insert((*dbxref)->GetTag().GetStr());
    }

    if (dbxrefs.empty())
        feat.ResetDbxref();
    else
        feat.SetDbxref().swap(dbxrefs);
}

/**********************************************************/
static void fta_collect_olts(GeneListPtr glp, CSeq_feat& feat)
{
    if(glp == NULL || !feat.IsSetQual())
        return;

    TQualVector quals;
    for (TQualVector::iterator qual = feat.SetQual().begin(); qual != feat.SetQual().end(); ++qual)
    {
        if (!(*qual)->IsSetQual() || !(*qual)->IsSetVal() ||
            (*qual)->GetQual() != "old_locus_tag")
        {
            quals.push_back(*qual);
            continue;
        }

        glp->olt.insert((*qual)->GetVal());
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
static void SrchGene(CSeq_annot::C_Data::TFtable& feats, GeneNodePtr gnp,
                     Int4 length, const CSeq_id& id)
{
    GeneListPtr newglp;
    char*     pseudogene;
    char*     locus_tag;
    char*     gene;

    if(gnp == NULL)
        return;

    NON_CONST_ITERATE(CSeq_annot::C_Data::TFtable, feat, feats)
    {
        gene = CpTheQualValue((*feat)->GetQual(), "gene");
        locus_tag = CpTheQualValue((*feat)->GetQual(), "locus_tag");

        const CSeq_loc* cur_loc = (*feat)->IsSetLocation() ? &(*feat)->GetLocation() : nullptr;
        if (gene == NULL && locus_tag == NULL)
        {
            if (GetFeatNameAndLoc(NULL, *(*feat), gnp))
                fta_append_feat_list(gnp, cur_loc, gene, locus_tag);
            continue;
        }

        pseudogene = CpTheQualValue((*feat)->GetQual(), "pseudogene");
        newglp = new GeneList;
        if (gene) {
            newglp->locus = gene;
        }
        if (locus_tag) {
            newglp->locus_tag = locus_tag;
        }

        if (pseudogene) {
            newglp->pseudogene = pseudogene;
        }
        fta_collect_wormbases(newglp, *(*feat));
        fta_collect_olts(newglp, *(*feat));
        if (GetFeatNameAndLoc(newglp, *(*feat), gnp))
            fta_append_feat_list(gnp, cur_loc, gene, locus_tag);

        newglp->feat.Reset();
        if (gnp->simple_genes == false && cur_loc != nullptr && cur_loc->IsMix())
        {
            newglp->feat.Reset(new CSeq_feat);
            newglp->feat->Assign(*(*feat));
        }

        newglp->slibp = GetLowHighFromSeqLoc(cur_loc, length, id);
        if(newglp->slibp == NULL)
        {
            MemFree(newglp);
            if(gene != NULL)
                MemFree(gene);
            if(locus_tag != NULL)
                MemFree(locus_tag);
            continue;
        }
        if(gnp->simple_genes == false && (*feat)->IsSetData() &&
           WeDontNeedToJoinThis((*feat)->GetData()))
            newglp->leave = 1;

        newglp->genefeat = IfCDSGeneFeat(*(*feat), CSeqFeatData::e_Gene, "gene");

       // newglp->maploc = ((*feat)->IsSetQual() ? GetTheQualValue((*feat)->SetQual(), "map") : NULL);
        if ((*feat)->IsSetQual()) {
            auto qual = GetTheQualValue((*feat)->SetQual(), "map");
            if (qual) {
                newglp->maploc = qual;
            } 
        }
        newglp->segnum = gnp->segindex;

        GetGeneSyns((*feat)->GetQual(), gene, newglp->syn);

        newglp->loc.Reset();
        if (cur_loc != nullptr)
        {
            newglp->loc.Reset(new CSeq_loc);
            newglp->loc->Assign(*cur_loc);
        }

        newglp->todel = false;
        if (IfCDSGeneFeat(*(*feat), CSeqFeatData::e_Cdregion, "CDS") == false && newglp->genefeat == false)
            newglp->pseudo = false;
        else
            newglp->pseudo = (*feat)->IsSetPseudo() ? (*feat)->GetPseudo() : false;

        newglp->allpseudo = (*feat)->IsSetPseudo() ? (*feat)->GetPseudo() : false;

        if(fta_rnas_cds_feat(*(*feat)))
        {
            newglp->noleft = newglp->slibp->noleft;
            newglp->noright = newglp->slibp->noright;
        }
        else
        {
            newglp->noleft = false;
            newglp->noright = false;
        }

        newglp->next = gnp->glp;
        gnp->glp = newglp;
    }

    if(gnp->gelop != NULL)
        gnp->gelop = fta_sort_feat_list(gnp->gelop);
}

/**********************************************************/
static CdssListPtr SrchCdss(CSeq_annot::C_Data::TFtable& feats, CdssListPtr clp,
                            Int4 segnum, const CSeq_id& id)
{
    CdssListPtr      newclp;
    SeqlocInfoblkPtr slip;

    ITERATE(CSeq_annot::C_Data::TFtable, feat, feats)
    {
        if (IfCDSGeneFeat(*(*feat), CSeqFeatData::e_Cdregion, "CDS") == false)
            continue;

        const CSeq_loc* cur_loc = (*feat)->IsSetLocation() ? &(*feat)->GetLocation() : nullptr;

        slip = GetLowHighFromSeqLoc(cur_loc, -99, id);
        if(slip == NULL)
            continue;

        newclp = new CdssList();
        newclp->segnum = segnum;
        newclp->from = slip->from;
        newclp->to = slip->to;
        delete slip;

        newclp->next = clp;
        clp = newclp;
    }

    return(clp);
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
    if (!bioseq.GetId().empty())
        first_id = *bioseq.GetId().begin();

    if (!first_id) {
        return;
    }

    if (IsSegBioseq(*first_id))
        return;                         /* process this bioseq */

    if (bioseq.GetInst().GetTopology() == CSeq_inst::eTopology_circular)
        gene_node->circular = true;

    if (!bioseq.IsSetAnnot())
        return;

    NON_CONST_ITERATE(CBioseq::TAnnot, annot, bioseq.SetAnnot())
    {
        if (!(*annot)->IsFtable())
            continue;

        CRef<CSeq_id> id = CpSeqIdAcOnly(*first_id, gene_node->accver);

        ++(gene_node->segindex);              /* > 1, if segment set */

        SrchGene((*annot)->SetData().SetFtable(), gene_node, bioseq.GetLength(), *id);

        if (gene_node->skipdiv)
            gene_node->clp = SrchCdss((*annot)->SetData().SetFtable(), gene_node->clp, gene_node->segindex, *id);

        if (gene_node->glp != NULL && gene_node->flag == false)
        {
            /* the seqentry is not a member of segment seqentry
             */
            gene_node->bioseq = &bioseq;
            gene_node->flag = true;
        }

        break;
    }
}

/**********************************************************/
static void GeneCheckForStrands(GeneListPtr glp)
{
    GeneListPtr tglp;

    if(glp == NULL)
        return;

    while(glp != NULL)
    {
        if(glp->locus.empty() && glp->locus_tag.empty())
            continue;
        bool got = false;
        for(tglp = glp->next; tglp != NULL; tglp = tglp->next)
        {
            if(tglp->locus.empty() && tglp->locus_tag.empty())
                continue;
            if(fta_cmp_locusyn(glp, tglp) != 0)
                break;
            if (!got && glp->slibp != NULL && tglp->slibp != NULL &&
               glp->slibp->strand != tglp->slibp->strand)
                got = true;
        }
        if(got)
        {
            ErrPostEx(SEV_WARNING, ERR_GENEREF_BothStrands,
                      "Gene name %s\"%s\" has been used for features on both strands.",
                      (glp->locus.empty()) ? "with locus_tag " : "",
                      (glp->locus.empty()) ? glp->locus_tag.c_str() : glp->locus.c_str());
        }
        glp = tglp;
    }
}

/**********************************************************/
static bool LocusTagCheck(GeneListPtr glp, bool& resort)
{
    GeneListPtr tglp;
    GeneListPtr glpstart;
    GeneListPtr glpstop;
    bool        same_gn;
    bool        same_lt;
    bool        ret;

    resort = false;
    if(glp == NULL || glp->next == NULL)
        return true;

    glpstop = NULL;
    for(ret = true; glp != NULL; glp = glpstop->next)
    {
        if(glp->locus.empty() && glp->locus_tag.empty())
            continue;

        glpstart = glp;
        glpstop = glp;
        for(tglp = glp->next; tglp != NULL; tglp = tglp->next)
        {
            if(NStr::EqualNocase(glp->locus, tglp->locus) == false ||
               NStr::EqualNocase(glp->locus_tag, tglp->locus_tag) == false)
                break;
            glpstop = tglp;
        }

        for(tglp = glpstop->next; tglp != NULL; tglp = tglp->next)
        {
            if(tglp->locus.empty() && tglp->locus_tag.empty())
                continue;

            same_gn = NStr::EqualNocase(glpstart->locus, tglp->locus);
            same_lt = NStr::EqualNocase(glpstart->locus_tag, tglp->locus_tag);

            if((same_gn == false && same_lt == false) || (same_gn && same_lt) ||
               same_gn || glpstart->locus_tag.empty())
                continue;

            for(glp = glpstart;; glp = glp->next)
            {
                ErrPostEx(SEV_REJECT, ERR_FEATURE_InconsistentLocusTagAndGene,
                          "Inconsistent pairs /gene+/locus_tag are encountered: \"%s\"+\"%s\" : %s feature at %s : \"%s\"+\"%s\" : %s feature at %s. Entry dropped.",
                          (glp->locus.empty()) ? "(NULL)" : glp->locus.c_str(),
                          (glp->locus_tag.empty()) ? "(NULL)" : glp->locus_tag.c_str(),
                          glp->fname.c_str(), glp->location.c_str(),
                          (tglp->locus.empty()) ? "(NULL)" : tglp->locus.c_str(),
                          (tglp->locus_tag.empty()) ? "(NULL)" : tglp->locus_tag.c_str(),
                          tglp->fname.c_str(), tglp->location.c_str());
                if(glp == glpstop)
                    break;
            }
            ret = false;
        }

        if(!glpstart->locus.empty()  && !glpstart->locus_tag.empty() &&
           NStr::EqualCase(glpstart->locus.c_str(), glpstart->locus_tag.c_str()))
        {
            for(glp = glpstart;; glp = glp->next)
            {
                glp->locus.clear();
                //MemFree(glp->locus);
                //glp->locus = NULL;
                resort = true;
                if(glp == glpstop)
                    break;
            }
        }
    }

    return(ret);
}

/**********************************************************/
static void MiscFeatsWithoutGene(GeneNodePtr gnp)
{
    GeneListPtr glp;
    GeneListPtr tglp;

    if(gnp == NULL || gnp->glp == NULL)
        return;

    for(glp = gnp->glp; glp != NULL; glp = glp->next)
    {
        if(glp->locus_tag.empty() || !glp->locus.empty() ||
           (glp->fname != "misc_feature"))
            continue;

        for(tglp = gnp->glp; tglp != NULL; tglp = tglp->next)
        {
            if(tglp->fname.empty() ||
                (tglp->fname == "misc_feature")) { // Looks suspicious - check again
                continue;
            }
            if(tglp->locus.empty() || tglp->locus[0] == '\0' ||
               fta_cmp_locus_tags(glp->locus_tag.c_str(), tglp->locus_tag.c_str()) != 0)
                continue;
            glp->locus = tglp->locus;
            break;
        }
    }
}

/**********************************************************/
static void RemoveUnneededMiscFeats(GeneNodePtr gnp)
{
    GeneListPtr glp;
    GeneListPtr glpprev;
    GeneListPtr glpnext;
    GeneListPtr tglp;

    if(gnp == NULL || gnp->glp == NULL)
        return;

    for(glp = gnp->glp; glp != NULL; glp = glp->next)
    {
        if(glp->todel || !glp->syn.empty() || (glp->fname != "misc_feature"))
            continue;

        bool got = false;
        for(tglp = gnp->glp; tglp != NULL; tglp = tglp->next)
        {
            if(tglp->todel || (tglp->fname == "misc_feature"))
                continue;
            if(fta_cmp_locus_tags(glp->locus.c_str(), tglp->locus.c_str()) != 0 ||
               fta_cmp_locus_tags(glp->locus_tag.c_str(), tglp->locus_tag.c_str()) != 0)
                continue;
            if(tglp->syn.empty())
            {
                got = true;
                continue;
            }

            sequence::ECompare cmp_res = sequence::Compare(*glp->loc, *tglp->loc, nullptr, sequence::fCompareOverlapping);
            if (cmp_res != sequence::eContained)
                continue;

            glp->todel = true;
        }
        if(glp->todel && got)
            glp->todel = false;
    }

    for(glpprev = NULL, glp = gnp->glp; glp != NULL; glp = glpnext)
    {
        glpnext = glp->next;
        if(!glp->todel)
        {
            glp->loc.Reset();
            glpprev = glp;
            continue;
        }

        if(glpprev == NULL)
            gnp->glp = glpnext;
        else
            glpprev->next = glpnext;

        glp->next = NULL;
        GeneListFree(glp);
    }
}

/**********************************************************/
static bool GeneLocusCheck(const TSeqFeatList& feats, bool diff_lt)
{
    bool ret = true;

    ITERATE(TSeqFeatList, feat, feats)
    {
        const CGene_ref& gene_ref1 = (*feat)->GetData().GetGene();
        if (!gene_ref1.IsSetLocus() || !gene_ref1.IsSetLocus_tag())
            continue;

        TSeqFeatList::const_iterator feat_next = feat,
                                     feat_cur = feat;
        for (++feat_next; feat_next != feats.end(); ++feat_next, ++feat_cur)
        {
            const CGene_ref& gene_ref2 = (*feat_next)->GetData().GetGene();

            if (!gene_ref2.IsSetLocus() || !gene_ref2.IsSetLocus_tag())
                continue;

            if (gene_ref1.GetLocus() != gene_ref2.GetLocus())
            {
                feat = feat_cur;
                break;
            }

            if (gene_ref1.GetLocus_tag() == gene_ref2.GetLocus_tag())
                continue;

            std::string loc1_str,
                        loc2_str;

            GetLocationStr((*feat)->GetLocation(), loc1_str);
            GetLocationStr((*feat_next)->GetLocation(), loc2_str);

            if(diff_lt == false)
            {
                ErrPostEx(SEV_REJECT,
                          ERR_FEATURE_MultipleGenesDifferentLocusTags,
                          "Multiple instances of the \"%s\" gene encountered: \"%s\"+\"%s\" : gene feature at \"%s\" : \"%s\"+\"%s\" : gene feature at \"%s\". Entry dropped.",
                          gene_ref1.GetLocus().c_str(), gene_ref1.GetLocus().c_str(), gene_ref1.GetLocus_tag().c_str(), loc1_str.c_str(),
                          gene_ref2.GetLocus().c_str(), gene_ref2.GetLocus_tag().c_str(), loc2_str.c_str());
                ret = false;
            }
            else
                ErrPostEx(SEV_WARNING,
                          ERR_FEATURE_MultipleGenesDifferentLocusTags,
                          "Multiple instances of the \"%s\" gene encountered: \"%s\"+\"%s\" : gene feature at \"%s\" : \"%s\"+\"%s\" : gene feature at \"%s\".",
                          gene_ref1.GetLocus().c_str(), gene_ref1.GetLocus().c_str(), gene_ref1.GetLocus_tag().c_str(), loc1_str.c_str(),
                          gene_ref2.GetLocus().c_str(), gene_ref2.GetLocus_tag().c_str(), loc2_str.c_str());
        }
    }

    return(ret);
}

/**********************************************************/
static void CheckGene(TEntryList& seq_entries, ParserPtr pp, GeneRefFeats& gene_refs)
{
    IndexblkPtr  ibp;
    GeneNodePtr  gnp;
    GeneListPtr  glp;

    char*      div;

    bool      resort;

    if(pp == NULL)
        return;

    ibp = pp->entrylist[pp->curindx];
    if(ibp == NULL)
        return;

    div = ibp->division;

    gnp = new GeneNode;
    gnp->accver = pp->accver;
    gnp->circular = false;
    gnp->simple_genes = pp->simple_genes;
    gnp->got_misc = false;
    if(div != NULL &&
       (StringCmp(div, "BCT") == 0 || StringCmp(div, "SYN") == 0))
        gnp->skipdiv = true;
    else
        gnp->skipdiv = false;

    NON_CONST_ITERATE(TEntryList, entry, seq_entries)
    {
        for (CTypeIterator<CBioseq> bioseq(Begin(*(*entry))); bioseq; ++bioseq)
        {
            FindGene(*bioseq, gnp);
        }

        for (CTypeIterator<CBioseq_set> bio_set(Begin(*(*entry))); bio_set; ++bio_set)
        {
            if (bio_set->GetClass() == CBioseq_set::eClass_parts)           /* parts, the place to put GeneRefPtr */
            {
                gnp->bioseq_set = &(*bio_set);
                gnp->flag = true;
                gnp->seg = true;
                break;
            }
        }
    }

    if (gnp->got_misc)
    {
        MiscFeatsWithoutGene(gnp);
        RemoveUnneededMiscFeats(gnp);
    }
    else
    {
        for(glp = gnp->glp; glp != NULL; glp = glp->next)
        {
            glp->loc.Reset();
        }
    }

    if(gnp->glp != NULL)
    {
        gnp = sort_gnp_list(gnp);

        resort = false;
        if(LocusTagCheck(gnp->glp, resort) == false)
        {
            ibp->drop = 1;
            GeneListFree(gnp->glp);
            CdssListFree(gnp->clp);
            GeneLocsFree(gnp->gelop);
            delete gnp;

            return;
        }

        if(resort)
            gnp = sort_gnp_list(gnp);

        ScannGeneName(gnp, gnp->bioseq == nullptr ? 0 : gnp->bioseq->GetLength());

        if(GeneLocusCheck(gnp->feats, pp->diff_lt) == false)
        {
            ibp->drop = 1;
            GeneListFree(gnp->glp);
            CdssListFree(gnp->clp);
            GeneLocsFree(gnp->gelop);

            delete gnp;

            return;
        }

        if(gnp->circular == false || ibp->got_plastid == false)
            GeneCheckForStrands(gnp->glp);

        if (!gnp->feats.empty())
        {
            CBioseq::TAnnot* annots = nullptr;
            if (gnp->seg)
            {
                annots = &gnp->bioseq_set->SetAnnot();
            }
            else
            {
                annots = &gnp->bioseq->SetAnnot();
            }

            NON_CONST_ITERATE(CBioseq::TAnnot, cur_annot, *annots)
            {
                if (!(*cur_annot)->IsFtable())
                    continue;

                size_t advance = (*cur_annot)->GetData().GetFtable().size();
                (*cur_annot)->SetData().SetFtable().splice((*cur_annot)->SetData().SetFtable().end(), gnp->feats);

                gene_refs.first = (*cur_annot)->SetData().SetFtable().begin();
                std::advance(gene_refs.first, advance);
                gene_refs.last = (*cur_annot)->SetData().SetFtable().end();
                gene_refs.valid = true;
                break;
            }

            if (annots->empty())
            {
                CRef<CSeq_annot> annot(new CSeq_annot);
                annot->SetData().SetFtable().assign(gnp->feats.begin(), gnp->feats.end());

                if (gnp->seg)
                {
                    gnp->bioseq_set->SetAnnot().push_back(annot);
                }
                else
                {
                    gnp->bioseq->SetAnnot().push_back(annot);
                }

                gene_refs.first = annot->SetData().SetFtable().begin();
                gene_refs.last = annot->SetData().SetFtable().end();
                gene_refs.valid = true;
            }
        }

        GeneListFree(gnp->glp);
        gnp->glp = NULL;
    }

    CdssListFree(gnp->clp);
    GeneLocsFree(gnp->gelop);

    delete gnp;
}

/**********************************************************
 *
 *   SeqFeatXrefPtr GetXrpForOverlap(glap, sfp, gerep):
 *
 *      Get xrp from list by locus only if cur gene overlaps
 *   other gene and asn2ff cannot find it.
 *
 **********************************************************/
static CRef<CSeqFeatXref> GetXrpForOverlap(const Char* acnum, GeneRefFeats& gene_refs, TSeqLocInfoList& llocs,
                                                                CSeq_feat& feat, CGene_ref& gerep)
{
    Int4           count = 0;
    Uint1          strand = feat.GetLocation().IsSetStrand() ? feat.GetLocation().GetStrand() : eNa_strand_unknown;

    if(strand == eNa_strand_other)
        strand = eNa_strand_unknown;

    CConstRef<CGene_ref> gene_ref;

    TSeqLocInfoList::const_iterator cur_loc = llocs.begin();
    CRef<CSeq_loc> loc = fta_seqloc_local(feat.GetLocation(), acnum);

    bool stopped = false;
    if (gene_refs.valid)
    {
        for (TSeqFeatList::iterator cur_feat = gene_refs.first; cur_feat != gene_refs.last; ++cur_feat)
        {
            sequence::ECompare cmp_res = sequence::Compare(*loc, *cur_loc->loc, nullptr, sequence::fCompareOverlapping);

            if (strand != cur_loc->strand ||
                (cmp_res != sequence::eContained && cmp_res != sequence::eSame))
            {
                ++cur_loc;
                continue;                   /* f location is within sfp one */
            }

            count++;
            if (gene_ref.Empty())
                gene_ref.Reset(&(*cur_feat)->GetData().GetGene());
            else if (fta_cmp_gene_refs(*gene_ref, (*cur_feat)->GetData().GetGene()) != 0)
            {
                stopped = true;
                break;
            }

            ++cur_loc;
        }
    }

    CRef<CSeqFeatXref> xref;

    if (count == 0 || (!stopped && gene_ref.NotEmpty() && fta_cmp_gene_refs(*gene_ref, gerep) == 0))
        return xref;

    xref.Reset(new CSeqFeatXref);
    xref->SetData().SetGene(gerep);

    return xref;
}

static void FixAnnot(CBioseq::TAnnot& annots, const Char* acnum, GeneRefFeats& gene_refs, TSeqLocInfoList& llocs)
{
    for (CBioseq::TAnnot::iterator annot = annots.begin(); annot != annots.end(); )
    {
        if (!(*annot)->IsSetData() || !(*annot)->GetData().IsFtable())
        {
            ++annot;
            continue;
        }

        CSeq_annot::C_Data::TFtable& feat_table = (*annot)->SetData().SetFtable();
        for (TSeqFeatList::iterator feat = feat_table.begin(); feat != feat_table.end();)
        {
            if ((*feat)->IsSetData() && (*feat)->GetData().IsImp())
            {
                const CImp_feat& imp = (*feat)->GetData().GetImp();
                if (imp.GetKey() == "gene")
                {
                    feat = feat_table.erase(feat);
                    continue;
                }
            }

            char* gene = (*feat)->IsSetQual() ? GetTheQualValue((*feat)->SetQual(), "gene") : NULL;
            char* locus_tag = (*feat)->IsSetQual() ? GetTheQualValue((*feat)->SetQual(), "locus_tag") : NULL;
            if (gene == NULL && locus_tag == NULL)
            {
                ++feat;
                continue;
            }

            CRef<CGene_ref> gene_ref(new CGene_ref);
            if (gene)
                gene_ref->SetLocus(gene);
            if (locus_tag)
                gene_ref->SetLocus_tag(locus_tag);

            TSynSet syns;
            GetGeneSyns((*feat)->GetQual(), gene, syns);
            if (!syns.empty())
                gene_ref->SetSyn().assign(syns.begin(), syns.end());

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
static void GeneQuals(TEntryList& seq_entries, const Char* acnum, GeneRefFeats& gene_refs)
{
    TSeqLocInfoList llocs;
    if (gene_refs.valid)
    {
        for (TSeqFeatList::iterator feat = gene_refs.first; feat != gene_refs.last; ++feat)
        {
            SeqLocInfo info;
            info.strand = (*feat)->GetLocation().IsSetStrand() ? (*feat)->GetLocation().GetStrand() : eNa_strand_unknown;

            if (info.strand == eNa_strand_other)
                info.strand = eNa_strand_unknown;

            info.loc = fta_seqloc_local((*feat)->GetLocation(), acnum);
            llocs.push_back(info);
        }
    }

    NON_CONST_ITERATE(TEntryList, entry, seq_entries)
    {
        for (CTypeIterator<CBioseq_set> bio_set(Begin(*(*entry))); bio_set; ++bio_set)
        {
            if (bio_set->IsSetAnnot())
                FixAnnot(bio_set->SetAnnot(), acnum, gene_refs, llocs);
        }

        for (CTypeIterator<CBioseq> bioseq(Begin(*(*entry))); bioseq; ++bioseq)
        {
            if (bioseq->IsSetAnnot())
                FixAnnot(bioseq->SetAnnot(), acnum, gene_refs, llocs);
        }
    }
}

/**********************************************************/
static void fta_collect_genes(const CBioseq& bioseq, std::set<std::string>& genes)
{
    ITERATE(CBioseq::TAnnot, annot, bioseq.GetAnnot())
    {
        if (!(*annot)->IsFtable())
            continue;

        ITERATE(CSeq_annot::C_Data::TFtable, feat, (*annot)->GetData().GetFtable())
        {
            ITERATE(CSeq_feat::TQual, qual, (*feat)->GetQual())
            {
                if (!(*qual)->IsSetQual() || (*qual)->GetQual() != "gene" ||
                    !(*qual)->IsSetVal() || (*qual)->GetVal().empty())
                    continue;

                genes.insert((*qual)->GetVal());
            }
        }
    }
}

/**********************************************************/
static void fta_fix_labels(CBioseq& bioseq, const std::set<std::string>& genes)
{
    if (!bioseq.IsSetAnnot())
        return;

    NON_CONST_ITERATE(CBioseq::TAnnot, annot, bioseq.SetAnnot())
    {
        if (!(*annot)->IsFtable())
            continue;

        NON_CONST_ITERATE(CSeq_annot::C_Data::TFtable, feat, (*annot)->SetData().SetFtable())
        {

            if (!(*feat)->IsSetQual())
                continue;

            NON_CONST_ITERATE(CSeq_feat::TQual, qual, (*feat)->SetQual())
            {
                if (!(*qual)->IsSetQual() || (*qual)->GetQual() != "label" ||
                    !(*qual)->IsSetVal() || (*qual)->GetVal().empty())
                    continue;

                const std::string& cur_val = (*qual)->GetVal();
                std::set<std::string>::const_iterator ci = genes.lower_bound(cur_val);
                if (*ci == cur_val)
                {
                    CRef<CGb_qual> new_qual(new CGb_qual);
                    new_qual->SetQual("gene");
                    new_qual->SetVal(cur_val);
                    
                    (*feat)->SetQual().insert(qual, new_qual);
                }
            }
        }
    }
}

/**********************************************************/
void DealWithGenes(TEntryList& seq_entries, ParserPtr pp)
{
    if(pp->source == Parser::ESource::Flybase)
    {
        std::set<std::string> genes;
        ITERATE(TEntryList, entry, seq_entries)
        {
            for (CBioseq_CI bioseq(GetScope(), *(*entry)); bioseq; ++bioseq)
            {
                fta_collect_genes(*bioseq->GetCompleteBioseq(), genes);
            }
        }

        if (!genes.empty())
        {
            NON_CONST_ITERATE(TEntryList, entry, seq_entries)
            {
                for (CTypeIterator<CBioseq> bioseq(Begin(*(*entry))); bioseq; ++bioseq)
                {
                    fta_fix_labels(*bioseq, genes);
                }
            }
        }
    }
    
    /* make GeneRefBlk if any gene qualifier exists
     */
    GeneRefFeats gene_refs;
    gene_refs.valid = false;
    CheckGene(seq_entries, pp, gene_refs);

    if (gene_refs.valid)
    {
        for (TSeqFeatList::iterator feat = gene_refs.first; feat != gene_refs.last; ++feat)
        {
            if ((*feat)->IsSetLocation())
            {
                int partial = sequence::SeqLocPartialCheck((*feat)->GetLocation(), &GetScope());
                if (partial & sequence::eSeqlocPartial_Start ||
                    partial & sequence::eSeqlocPartial_Stop) // not internal
                    (*feat)->SetPartial(true);

                if (!pp->genenull || !(*feat)->GetLocation().IsMix())
                    continue;

                CSeq_loc_mix& mix_loc = (*feat)->SetLocation().SetMix();

                CRef<CSeq_loc> null_loc(new CSeq_loc);
                null_loc->SetNull();

                CSeq_loc_mix::Tdata::iterator it_loc = mix_loc.Set().begin();
                ++it_loc;
                for (; it_loc != mix_loc.Set().end(); ++it_loc)
                {
                    it_loc = mix_loc.Set().insert(it_loc, null_loc);
                    ++it_loc;
                }
            }
        }
    }

    ProcNucProt(pp, seq_entries, gene_refs);

    /* remove /gene if they can be mapped to GenRef
     */
    if (!seq_entries.empty())
        GeneQuals(seq_entries, pp->entrylist[pp->curindx]->acnum, gene_refs);
}

END_NCBI_SCOPE
